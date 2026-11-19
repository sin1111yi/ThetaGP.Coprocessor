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

// One frame of the wire: a varint length, the payload, and the payload's byte
// sum in the low 16 bits, little endian. Nothing here touches a bus: bytes go
// in and a frame comes out, so the codec is the same on the board and on a
// desk.

#pragma once

#include <cstdint>

namespace ThetaGP::Drivers::Link::Frame {

// The most one payload carries, and the most a frame can take on the wire: two
// bytes of prefix for the length, the payload, two of checksum.
inline constexpr uint16_t PAYLOAD_MAX = 1024;
inline constexpr uint16_t FRAME_MAX = 2 + PAYLOAD_MAX + 2;

// Writes one payload into `out` as a frame. Returns the bytes written, or 0
// when the payload is longer than a frame carries or `capacity` is too small.
uint16_t encode(const uint8_t *payload, uint16_t length, uint8_t *out,
                uint16_t capacity);

// Reads one frame a byte at a time. `feed` is true only for the byte that
// completes a frame whose sum agrees; what the parser holds is that frame's
// until the next byte arrives.
class Parser {
public:
  bool feed(uint8_t byte);
  void reset(void);

  const uint8_t *payload(void) const { return _payload; }
  uint16_t length(void) const { return _completed; }

  // Whole frames taken and refused since boot, as the status page reads them.
  uint32_t received(void) const { return _received; }
  uint32_t refused(void) const { return _refused; }

private:
  // The three steps a frame is read in.
  uint8_t _state = 0;

  uint8_t _payload[PAYLOAD_MAX] = {};
  uint16_t _length = 0;
  uint16_t _completed = 0;
  uint16_t _filled = 0;
  uint16_t _sum = 0;
  uint8_t _prefixBytes = 0;
  uint8_t _checksumLow = 0;
  uint8_t _checksumBytes = 0;

  volatile uint32_t _received = 0;
  volatile uint32_t _refused = 0;
};

// Proves the two directions of the framing agree and prints the one line the
// boot log carries: a link that never answers should be a wire, not a codec.
void selfCheck(void);

} // namespace ThetaGP::Drivers::Link::Frame
