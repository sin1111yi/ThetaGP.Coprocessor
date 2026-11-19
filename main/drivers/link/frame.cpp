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

#include "drivers/link/frame.h"

#include <cstdio>

namespace ThetaGP::Drivers::Link::Frame {

namespace {

constexpr uint8_t kPrefix = 0;
constexpr uint8_t kPayload = 1;
constexpr uint8_t kChecksum = 2;

uint16_t payloadSum(const uint8_t *payload, uint16_t length) {
  uint16_t sum = 0;
  for (uint16_t i = 0; i < length; ++i) {
    sum = static_cast<uint16_t>(sum + payload[i]);
  }
  return sum;
}

} // namespace

uint16_t encode(const uint8_t *payload, uint16_t length, uint8_t *out,
                uint16_t capacity) {
  const uint8_t prefix = length < 0x80u ? 1 : 2;
  const uint16_t total =
      static_cast<uint16_t>(prefix + length + sizeof(uint16_t));
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
  const uint16_t sum = payloadSum(payload, length);
  out[used++] = static_cast<uint8_t>(sum & 0xFFu);
  out[used++] = static_cast<uint8_t>((sum >> 8) & 0xFFu);
  return used;
}

void Parser::reset(void) {
  _length = 0;
  _filled = 0;
  _sum = 0;
  _prefixBytes = 0;
  _checksumLow = 0;
  _checksumBytes = 0;
  _state = kPrefix;
}

bool Parser::feed(uint8_t byte) {
  switch (_state) {
  case kPrefix:
    _length = _prefixBytes == 0
                  ? (uint16_t)(byte & 0x7Fu)
                  : (uint16_t)(_length | ((uint16_t)(byte & 0x7Fu) << 7));
    if (_prefixBytes == 0 && (byte & 0x80u) != 0) {
      // A length this small is one byte: anything else continues it.
      _prefixBytes = 1;
      return false;
    }
    if (_length > PAYLOAD_MAX) {
      // A length no frame could carry: give up on it and let the next byte
      // start again.
      _refused = _refused + 1;
      reset();
      return false;
    }
    _filled = 0;
    _sum = 0;
    _checksumBytes = 0;
    _state = _length == 0 ? kChecksum : kPayload;
    return false;

  case kPayload:
    _payload[_filled++] = byte;
    _sum = static_cast<uint16_t>(_sum + byte);
    if (_filled == _length) {
      _state = kChecksum;
      _checksumBytes = 0;
    }
    return false;

  case kChecksum:
    if (_checksumBytes == 0) {
      _checksumLow = byte;
      _checksumBytes = 1;
      return false;
    }
    if (_sum != (uint16_t)(_checksumLow | ((uint16_t)byte << 8))) {
      _refused = _refused + 1;
      reset();
      return false;
    }
    if (_length == 0) {
      // An idle or floating line reads as zero bytes, and three of them look
      // like an empty frame. Nothing carries a payload of none.
      _refused = _refused + 1;
      reset();
      return false;
    }
    _received = _received + 1;
    // The frame's length is the parser's until the reset below clears it.
    _completed = _length;
    reset();
    return true;

  default:
    reset();
    return false;
  }
}

void selfCheck(void) {
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

} // namespace ThetaGP::Drivers::Link::Frame
