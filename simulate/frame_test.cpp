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

// The wire, on a desk: a payload written and read back byte for byte, the queue
// the two tasks meet in, and the frames that must not be taken.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "wire/frame.h"

namespace {

using ThetaGP::Wire::Frame;

int s_failed;
int s_checked;

void check(bool ok, const char *what) {
  ++s_checked;
  if (!ok) {
    ++s_failed;
    std::printf("frame: FAIL %s\n", what);
  }
}

// Writes one payload into `framed`; 0 when the codec refused it.
uint16_t write(const uint8_t *payload, uint16_t length, uint8_t *framed,
               uint16_t capacity) {
  return Frame::encode(payload, length, framed, capacity);
}

// Feeds a frame in and returns what was taken, if anything.
bool read(Frame &frame, const uint8_t *bytes, uint16_t length, uint32_t nowMs,
          Frame::Payload &out) {
  frame.feed(bytes, length, nowMs);
  return frame.take(out);
}

} // namespace

int main() {
  const uint8_t sample[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x7F, 0x80};
  uint8_t many[Frame::PAYLOAD_MAX] = {};
  for (uint16_t i = 0; i < Frame::PAYLOAD_MAX; ++i) {
    many[i] = static_cast<uint8_t>(i * 7 + 3);
  }
  uint8_t framed[Frame::FRAME_MAX] = {};
  Frame::Payload out = {};

  // ── what a frame carries comes back ──

  struct Case {
    const uint8_t *bytes;
    uint16_t length;
    const char *what;
  };
  const Case cases[] = {
      {sample, sizeof sample, "seven bytes"},
      {many, 1, "one byte"},
      {many, 127, "127 bytes"},
      {many, 128, "128 bytes"},
      {many, 300, "300 bytes"},
      {many, Frame::PAYLOAD_MAX, "a full payload"},
  };

  uint32_t at = 0;
  for (const Case &item : cases) {
    Frame frame;
    const uint16_t size = write(item.bytes, item.length, framed, sizeof framed);
    check(size != 0, "a payload is written");
    const bool taken = read(frame, framed, size, at += 10, out);
    check(taken, item.what);
    check(taken && out.length == item.length, item.what);
    check(taken && std::memcmp(out.bytes, item.bytes, item.length) == 0,
          item.what);
    check(frame.droppedFrames() == 0, "nothing refused");
  }

  // ── a payload no frame carries is not written ──

  check(write(many, Frame::PAYLOAD_MAX + 1, framed, sizeof framed) == 0,
        "an oversized payload is refused");
  check(write(sample, sizeof sample, framed, sizeof sample) == 0,
        "a capacity too small is refused");
  check(write(nullptr, 3, framed, sizeof framed) == 0,
        "a null payload is refused");

  // ── the queue is where the two tasks meet ──

  {
    Frame frame;
    uint8_t one[Frame::FRAME_MAX] = {};
    uint8_t two[Frame::FRAME_MAX] = {};
    const uint16_t a = write(sample, sizeof sample, one, sizeof one);
    const uint16_t b = write(many, 300, two, sizeof two);
    frame.feed(one, a, 10);
    frame.feed(two, b, 20);
    check(frame.queued() == 2, "two frames wait");
    check(frame.take(out) && out.length == sizeof sample, "the oldest first");
    check(frame.take(out) && out.length == 300, "then the next");
    check(!frame.take(out), "and then none");
    check(frame.queued() == 0, "the queue is empty");
  }

  {
    // Every slot taken: the frame that cannot be held is refused and counted,
    // and the queue keeps what it had.
    Frame frame;
    const uint16_t size = write(sample, sizeof sample, framed, sizeof framed);
    for (uint8_t i = 0; i < Frame::SLOTS; ++i) {
      frame.feed(framed, size, 10 + i);
    }
    check(frame.queued() == Frame::SLOTS, "every slot is filled");
    frame.feed(framed, size, 100);
    check(frame.lastDrop() == Frame::Drop::NoRoom, "no room is a refusal");
    check(frame.droppedFrames() == 1, "the refusal is counted");
    check(frame.queued() == Frame::SLOTS, "what was held is kept");
  }

  // ── and what arrives broken is counted, not taken ──

  {
    Frame frame;
    const uint16_t size = write(sample, sizeof sample, framed, sizeof framed);
    framed[size - 1] ^= 0xFFu; // a checksum that no longer agrees
    check(!read(frame, framed, size, 10, out), "a bad sum is not a frame");
    check(frame.lastDrop() == Frame::Drop::Checksum, "a bad sum is its own drop");
    check(frame.droppedFrames() == 1, "a bad sum is counted");
    check(frame.queued() == 0, "a bad sum is not taken");
  }

  {
    // A third prefix byte: a length above PAYLOAD_MAX whatever it says.
    Frame frame;
    const uint8_t prefix[3] = {0xFFu, 0xFFu, 0x01u};
    check(!read(frame, prefix, sizeof prefix, 10, out),
          "a long prefix is not a frame");
    check(frame.lastDrop() == Frame::Drop::Prefix, "the prefix is the drop");
    check(frame.droppedFrames() == 1, "the prefix is counted");
  }

  {
    // A two-byte length above the payload: refused on the length.
    Frame frame;
    const uint8_t prefix[2] = {
        static_cast<uint8_t>(((Frame::PAYLOAD_MAX + 1) & 0x7Fu) | 0x80u),
        static_cast<uint8_t>(((Frame::PAYLOAD_MAX + 1) >> 7) & 0x7Fu)};
    check(!read(frame, prefix, sizeof prefix, 10, out),
          "an oversized length is not a frame");
    check(frame.lastDrop() == Frame::Drop::Length, "the length is the drop");
  }

  {
    // An idle line reads as zeroes, and three of them look like an empty
    // frame: nothing carries a payload of none.
    Frame frame;
    const uint8_t idle[3] = {0x00, 0x00, 0x00};
    check(!read(frame, idle, sizeof idle, 10, out),
          "an empty frame is not a frame");
    check(frame.droppedFrames() == 1, "an empty frame is counted");

    // The bytes after it are still read as the next length prefix.
    const uint16_t size = write(sample, sizeof sample, framed, sizeof framed);
    check(read(frame, framed, size, 20, out), "the line recovers");
  }

  {
    // A half frame whose bytes stopped arriving is dropped, and the byte after
    // the silence starts a new one.
    Frame frame;
    const uint16_t size = write(sample, sizeof sample, framed, sizeof framed);
    frame.feed(framed, 3, 100); // a length, a payload byte or two, no more
    check(frame.halfFrame(), "a half frame is half a frame");
    check(frame.declaredLength() == sizeof sample, "the length is declared");
    check(!frame.tick(100 + Frame::IDLE_TIMEOUT_MS - 1),
          "a half frame is given its time");
    check(frame.tick(100 + Frame::IDLE_TIMEOUT_MS), "then it is dropped");
    check(frame.lastDrop() == Frame::Drop::Idle, "the silence is the drop");
    check(!frame.halfFrame(), "the prefix is being read again");
    check(read(frame, framed, size, 5000, out), "the next frame lands");
  }

  // ── two frames in one read ──

  {
    Frame frame;
    uint8_t both[2 * Frame::FRAME_MAX] = {};
    const uint16_t a = write(sample, sizeof sample, both, sizeof both);
    const uint16_t b = write(many, 300, both + a, sizeof both - a);
    check(a != 0 && b != 0, "two frames written");
    frame.feed(both, static_cast<uint16_t>(a + b), 10);
    check(frame.queued() == 2, "both wait");
    check(frame.take(out) && out.length == sizeof sample, "the first one");
    check(frame.take(out) && out.length == 300, "the second one");
  }

  std::printf("frame: %d checks, %d failed\n", s_checked, s_failed);
  return s_failed == 0 ? 0 : 1;
}
