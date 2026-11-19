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

// The framing, on a desk: a payload written and read back byte for byte, and
// the frames that must not be taken.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "drivers/link/frame.h"

namespace {

using ThetaGP::Drivers::Link::Frame::FRAME_MAX;
using ThetaGP::Drivers::Link::Frame::PAYLOAD_MAX;
using ThetaGP::Drivers::Link::Frame::Parser;

int s_failed;
int s_checked;

void check(bool ok, const char *what) {
  ++s_checked;
  if (!ok) {
    ++s_failed;
    std::printf("frame: FAIL %s\n", what);
  }
}

// Writes one payload, then reads the frame back a byte at a time.
bool roundTrip(Parser &parser, const uint8_t *payload, uint16_t length,
               const char *what) {
  uint8_t framed[FRAME_MAX] = {};
  const uint16_t size = ThetaGP::Drivers::Link::Frame::encode(
      payload, length, framed, sizeof framed);
  if (size == 0) {
    std::printf("frame: FAIL encode %s\n", what);
    ++s_failed;
    return false;
  }

  bool landed = false;
  for (uint16_t i = 0; i < size; ++i) {
    if (parser.feed(framed[i])) {
      landed = true;
      check(parser.length() == length, what);
      check(std::memcmp(parser.payload(), payload, length) == 0, what);
    }
  }
  check(landed, what);
  return landed;
}

} // namespace

int main() {
  // ── what a frame carries comes back ──

  const uint8_t small[] = {0x42};
  const uint8_t sample[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x7F, 0x80};

  uint8_t many[PAYLOAD_MAX] = {};
  for (uint16_t i = 0; i < PAYLOAD_MAX; ++i) {
    many[i] = static_cast<uint8_t>(i * 7 + 3);
  }

  Parser parser;
  parser.reset();
  const uint32_t before = parser.received();
  roundTrip(parser, small, sizeof small, "one byte");
  roundTrip(parser, sample, sizeof sample, "seven bytes");
  roundTrip(parser, many, 127, "127 bytes");
  roundTrip(parser, many, 128, "128 bytes");
  roundTrip(parser, many, 300, "300 bytes");
  roundTrip(parser, many, PAYLOAD_MAX, "a full payload");
  check(parser.received() == before + 6, "every frame counted");
  check(parser.refused() == 0, "nothing refused");

  // ── a payload no frame carries is not written ──

  uint8_t framed[FRAME_MAX] = {};
  check(ThetaGP::Drivers::Link::Frame::encode(many, PAYLOAD_MAX + 1, framed,
                                             sizeof framed) == 0,
        "an oversized payload is refused");
  check(ThetaGP::Drivers::Link::Frame::encode(sample, sizeof sample, framed,
                                             sizeof sample) == 0,
        "a capacity too small is refused");

  // ── and what arrives broken is counted, not taken ──

  {
    Parser broken;
    broken.reset();
    const uint16_t size = ThetaGP::Drivers::Link::Frame::encode(
        sample, sizeof sample, framed, sizeof framed);
    framed[size - 1] ^= 0xFFu; // a checksum that no longer agrees
    for (uint16_t i = 0; i < size; ++i) {
      check(!broken.feed(framed[i]), "a bad sum is not a frame");
    }
    check(broken.refused() == 1, "a bad sum is counted");
    check(broken.received() == 0, "a bad sum is not received");
  }

  {
    // A length longer than any frame, in the two bytes a varint takes.
    Parser oversized;
    oversized.reset();
    const uint8_t prefix[2] = {
        static_cast<uint8_t>(((PAYLOAD_MAX + 1) & 0x7Fu) | 0x80u),
        static_cast<uint8_t>(((PAYLOAD_MAX + 1) >> 7) & 0x7Fu)};
    check(!oversized.feed(prefix[0]), "an oversized length is not a frame");
    check(!oversized.feed(prefix[1]), "an oversized length is not a frame");
    check(oversized.refused() == 1, "an oversized length is counted");
    // The bytes after it are still read as the start of the next frame.
    roundTrip(oversized, sample, sizeof sample, "the line recovers");
  }

  {
    // An idle line reads as zeroes, and three of them look like an empty
    // frame: nothing carries a payload of none.
    Parser idle;
    idle.reset();
    check(!idle.feed(0x00), "an empty frame is not a frame");
    check(!idle.feed(0x00), "an empty frame is not a frame");
    check(!idle.feed(0x00), "an empty frame is not a frame");
    check(idle.refused() == 1, "an empty frame is counted");
    check(idle.received() == 0, "an empty frame is not received");
  }

  {
    // Two frames in one read, the second of them after rubbish.
    Parser stream;
    stream.reset();
    uint8_t both[3 * FRAME_MAX] = {};
    const uint16_t one = ThetaGP::Drivers::Link::Frame::encode(
        sample, sizeof sample, both, sizeof both);
    const uint16_t two = ThetaGP::Drivers::Link::Frame::encode(
        small, sizeof small, both + one, sizeof both - one);
    check(one != 0 && two != 0, "two frames written");
    int landed = 0;
    for (uint16_t i = 0; i < (uint16_t)(one + two); ++i) {
      if (stream.feed(both[i])) {
        ++landed;
      }
    }
    check(landed == 2, "two frames in one read");
    check(stream.received() == 2, "both counted");
  }

  std::printf("frame: %d checks, %d failed\n", s_checked, s_failed);
  return s_failed == 0 ? 0 : 1;
}
