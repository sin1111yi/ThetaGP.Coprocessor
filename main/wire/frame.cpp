/**
 * This file is a part of ThetaGP.
 *
 * ThetaGP is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * ThetaGP is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.
 *
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "wire/frame.h"

#include <cstdio>
#include <cstring>

namespace ThetaGP::Wire {

uint16_t Frame::checksum(const uint8_t *payload, uint16_t length) {
  uint16_t sum = 0;
  for (uint16_t i = 0; i < length; ++i) {
    sum = static_cast<uint16_t>(sum + payload[i]);
  }
  return sum;
}

uint8_t Frame::prefixSize(uint16_t length) {
  return length < 0x80u ? 1 : 2;
}

uint16_t Frame::encode(const uint8_t *payload, uint16_t length, uint8_t *out,
                       uint16_t capacity) {
  if (out == nullptr || (payload == nullptr && length != 0)) {
    return 0;
  }
  const uint8_t prefix = prefixSize(length);
  const uint16_t total =
      static_cast<uint16_t>(prefix + length + CHECKSUM_BYTES);
  if (length > PAYLOAD_MAX || total > capacity) {
    return 0;
  }

  uint16_t used = 0;
  if (prefix == 1) {
    out[used++] = static_cast<uint8_t>(length);
  } else {
    out[used++] = static_cast<uint8_t>((length & 0x7Fu) | 0x80u);
    out[used++] = static_cast<uint8_t>((length >> 7) & 0x7Fu);
  }
  for (uint16_t i = 0; i < length; ++i) {
    out[used++] = payload[i];
  }
  const uint16_t sum = checksum(payload, length);
  out[used++] = static_cast<uint8_t>(sum & 0xFFu);
  out[used++] = static_cast<uint8_t>((sum >> 8) & 0xFFu);
  return used;
}

void Frame::reset(void) {
  _state = State::Prefix;
  _prefixLen = 0;
  _length = 0;
  _used = 0;
  _sum = 0;
  _checksumLen = 0;
}

void Frame::drop(Drop reason) {
  _lastDrop = reason;
  reset();
  // Written on the transport's task and read on the task above as a count of
  // what went wrong, never as a decision.
  _dropped = _dropped + 1;
}

void Frame::feed(const uint8_t *bytes, uint16_t length, uint32_t nowMs) {
  for (uint16_t i = 0; i < length; ++i) {
    const uint8_t byte = bytes[i];
    _lastByteMs = nowMs;

    switch (_state) {
    case State::Prefix: {
      // A third prefix byte declares a length above PAYLOAD_MAX whatever its
      // own bits say, so it is refused where it arrives.
      if (_prefixLen == PREFIX_MAX_BYTES) {
        drop(Drop::Prefix);
        break;
      }
      _prefix[_prefixLen] = byte;
      ++_prefixLen;
      if ((byte & 0x80u) != 0) {
        break; // the length continues in the byte after this one
      }

      uint16_t declared = 0;
      for (uint8_t at = 0; at < _prefixLen; ++at) {
        declared = static_cast<uint16_t>(
            declared | (static_cast<uint16_t>(_prefix[at] & 0x7Fu)
                        << (7u * at)));
      }
      if (declared > PAYLOAD_MAX) {
        drop(Drop::Length);
        break;
      }

      _length = declared;
      _used = 0;
      _sum = 0;
      _checksumLen = 0;
      _state = declared == 0 ? State::Checksum : State::Payload;
      break;
    }

    case State::Payload:
      _payload[_used] = byte;
      ++_used;
      _sum = static_cast<uint16_t>(_sum + byte);
      if (_used == _length) {
        _state = State::Checksum;
      }
      break;

    case State::Checksum:
      _checksum[_checksumLen] = byte;
      ++_checksumLen;
      if (_checksumLen < CHECKSUM_BYTES) {
        break;
      }
      if (checksumOfChecksumBytes() != _sum) {
        // The payload never travels: the frame is refused whole and the byte
        // after it is read as a new length prefix.
        drop(Drop::Checksum);
        break;
      }
      if (_length == 0) {
        // An idle or floating line reads as zero bytes, and three of them look
        // like an empty frame. Nothing carries a payload of none.
        drop(Drop::Length);
        break;
      }
      publish();
      break;
    }
  }
}

uint16_t Frame::checksumOfChecksumBytes(void) const {
  return static_cast<uint16_t>(_checksum[0] |
                               (static_cast<uint16_t>(_checksum[1]) << 8));
}

bool Frame::tick(uint32_t nowMs) {
  // Only a length that was declared can be half a frame: a prefix that promised
  // another prefix byte declares nothing yet and is left where it is.
  if (!halfFrame()) {
    return false;
  }
  if (static_cast<uint32_t>(nowMs - _lastByteMs) < IDLE_TIMEOUT_MS) {
    return false;
  }
  drop(Drop::Idle);
  return true;
}

void Frame::publish(void) {
  const uint8_t tail = _tail.load(std::memory_order_relaxed);
  const uint8_t next = static_cast<uint8_t>((tail + 1) % (SLOTS + 1));
  if (next == _head.load(std::memory_order_acquire)) {
    // A whole frame whose checksum held, with every slot taken: the frame is
    // refused and counted, and the stream goes on at the byte after it.
    drop(Drop::NoRoom);
    return;
  }

  Slot &slot = _slots[tail];
  slot.length = _length;
  std::memcpy(slot.bytes, _payload, _length);
  _tail.store(next, std::memory_order_release);
  reset();
}

bool Frame::take(Payload &out) {
  const uint8_t head = _head.load(std::memory_order_relaxed);
  if (head == _tail.load(std::memory_order_acquire)) {
    return false;
  }
  const Slot &slot = _slots[head];
  out.bytes = slot.bytes;
  out.length = slot.length;
  _head.store(static_cast<uint8_t>((head + 1) % (SLOTS + 1)),
              std::memory_order_release);
  return true;
}

uint8_t Frame::queued(void) const {
  const uint8_t head = _head.load(std::memory_order_relaxed);
  const uint8_t tail = _tail.load(std::memory_order_relaxed);
  return static_cast<uint8_t>((tail + SLOTS + 1 - head) % (SLOTS + 1));
}

uint32_t Frame::droppedFrames(void) const { return _dropped; }

Frame::Drop Frame::lastDrop(void) const { return _lastDrop; }

Frame::State Frame::state(void) const { return _state; }

bool Frame::halfFrame(void) const { return _state != State::Prefix; }

uint16_t Frame::declaredLength(void) const { return _length; }

void Frame::selfCheck(void) {
  const uint8_t sample[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x7F, 0x80};
  uint8_t framed[FRAME_MAX] = {};
  const uint16_t size = encode(sample, sizeof sample, framed, sizeof framed);
  if (size == 0) {
    printf("link: codec broken (encode)\n");
    return;
  }

  uint16_t sum = 0;
  for (uint16_t i = 0; i < sizeof sample; ++i) {
    sum = static_cast<uint16_t>(sum + sample[i]);
  }
  const bool ok = framed[0] == sizeof sample &&
                  framed[size - 1] == (sum >> 8) &&
                  framed[size - 2] == (sum & 0xFF);
  printf("link: codec %s\n", ok ? "ok" : "broken");
}

} // namespace ThetaGP::Wire
