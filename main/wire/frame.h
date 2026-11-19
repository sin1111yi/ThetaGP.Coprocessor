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

// One frame of the link: a varint length, the payload, and the payload's byte
// sum in the low 16 bits, little endian. The length and the checksum both
// cover the payload alone, so neither counts the prefix.
//
// The class carries no message and no message type: a payload is handed up as
// bytes, and what is inside it belongs to the layer above. It also reaches no
// peripheral: the UART task feeds it the bytes it read, and the task that owns
// the widgets takes whole frames out, which is why the two meet in a queue
// rather than in a call.
//
// The same code runs on the main board, where the CDC interrupt feeds it and
// the main loop takes from it.

#pragma once

#include <atomic>
#include <cstdint>

namespace ThetaGP::Wire {

class Frame {
public:
  // Why a frame was refused. The codec names the step and carries no sentence.
  enum class Drop : uint8_t {
    None = 0, // Nothing has been refused.
    Prefix,   // A length prefix of more than PREFIX_MAX_BYTES bytes.
    Length,   // A length above PAYLOAD_MAX, or one declaring an empty frame.
    Checksum, // The checksum is not the payload's sum.
    Idle,     // A declared length whose bytes stopped arriving.
    NoRoom,   // A whole frame with every slot taken.
  };

  // The step the receive side is in.
  enum class State : uint8_t {
    Prefix,   // Reading a length prefix.
    Payload,  // Reading the payload the prefix declared.
    Checksum, // Reading the checksum's two bytes.
  };

  // One frame's payload, as it is handed to the layer above.
  struct Payload {
    const uint8_t *bytes;
    uint16_t length;
  };

  // Largest payload one frame carries, and the most bytes it takes on the
  // wire: a two-byte length, the payload, the checksum.
  static constexpr uint16_t PAYLOAD_MAX = 1024;
  static constexpr uint16_t FRAME_MAX = 2 + PAYLOAD_MAX + 2;
  // A prefix is one byte below 128 and two from 128 up; the checksum is two.
  static constexpr uint8_t PREFIX_MAX_BYTES = 2;
  static constexpr uint8_t CHECKSUM_BYTES = 2;
  // Whole frames held for the task above, and the idle time a declared length
  // is given to arrive in before the half frame is dropped.
  static constexpr uint8_t SLOTS = 4;
  static constexpr uint32_t IDLE_TIMEOUT_MS = 1000;

  Frame() = default;

  // ── the send side ──

  // Sum of a payload's bytes in the low 16 bits, wrapping there.
  static uint16_t checksum(const uint8_t *payload, uint16_t length);

  // Bytes a length takes as a prefix.
  static uint8_t prefixSize(uint16_t length);

  // Writes one frame into `out` and returns its size, or 0 when the payload is
  // above PAYLOAD_MAX, a pointer is null, or `out` is too short.
  static uint16_t encode(const uint8_t *payload, uint16_t length, uint8_t *out,
                         uint16_t capacity);

  // Proves the two directions of the framing agree and prints the one line the
  // boot log carries: a link that never answers should be a wire, not a codec.
  static void selfCheck(void);

  // ── the receive side ──

  // Feeds the bytes the transport read, stamped with their arrival time for
  // the idle timeout. Called on the transport's task.
  void feed(const uint8_t *bytes, uint16_t length, uint32_t nowMs);

  // Drops a half frame whose bytes stopped arriving. Called on the transport's
  // task; true when this call dropped one.
  bool tick(uint32_t nowMs);

  // Takes the oldest whole frame whose checksum held. Called on the task above;
  // the bytes are that slot's until it is handed out again.
  bool take(Payload &out);

  // Whole frames waiting to be taken.
  uint8_t queued(void) const;

  // Frames refused since power-on.
  uint32_t droppedFrames(void) const;

  // Why the frame refused last was refused.
  Drop lastDrop(void) const;

  // The step the receive side is in.
  State state(void) const;

  // Whether a length is declared and its payload and checksum are not all in.
  bool halfFrame(void) const;

  // The length the prefix of a half frame declared.
  uint16_t declaredLength(void) const;

private:
  // One whole frame held for the layer above.
  struct Slot {
    uint16_t length;
    uint8_t bytes[PAYLOAD_MAX];
  };

  // Hand the assembled frame up and free the assembler for the next one.
  void publish(void);
  // Refuse what the assembler holds, count it, and read the next byte as a
  // length prefix.
  void drop(Drop reason);
  // Back to reading a length prefix; the slots and the counter are not touched.
  void reset(void);
  // The two checksum bytes just arrived, low byte first, as the number they are.
  uint16_t checksumOfChecksumBytes(void) const;

  // One slot is left empty so that a producer and a consumer on two cores need
  // no lock between them: each index has one writer.
  Slot _slots[SLOTS + 1] = {};
  std::atomic<uint8_t> _head{0};
  std::atomic<uint8_t> _tail{0};

  State _state = State::Prefix;
  uint8_t _prefix[PREFIX_MAX_BYTES] = {0};
  uint8_t _prefixLen = 0;
  uint16_t _length = 0;
  uint8_t _payload[PAYLOAD_MAX] = {0};
  uint16_t _used = 0;
  uint16_t _sum = 0;
  uint8_t _checksum[CHECKSUM_BYTES] = {0};
  uint8_t _checksumLen = 0;
  uint32_t _lastByteMs = 0;

  volatile uint32_t _dropped = 0;
  Drop _lastDrop = Drop::None;
};

} // namespace ThetaGP::Wire
