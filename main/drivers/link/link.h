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

// The wire to the main board: a varint length, the payload, and the payload's
// byte sum in the low 16 bits, little endian, carried over one UART. The layer
// knows nothing of messages: bytes go up, bytes come down, and the ready line
// says the decoder is listening.

#pragma once

#include <cstdint>

namespace ThetaGP::Drivers::Link {

// One payload, as it arrives from the main board.
using FrameHandler = void (*)(const uint8_t *payload, uint16_t length,
                              void *context);

class Link {
public:
  static Link &getInstance() {
    static Link instance;
    return instance;
  }

  // Claims the pins and the UART, then starts the task that reads it.
  void init();

  // Raises the line the main board waits on before it sends anything.
  void markReady();

  // Sends one payload, framed. Returns false if it is too long or the bus is
  // still carrying the previous one.
  bool send(const uint8_t *payload, uint16_t length);

  void setHandler(FrameHandler handler, void *context = nullptr) {
    _handler = handler;
    _context = context;
  }

  // Whole frames taken and refused since boot, as the status page reads them.
  uint32_t received() const { return _received; }
  uint32_t refused() const { return _refused; }

  // True while frames are still arriving: a beat that stopped a second and a
  // half ago is a link that is down, not one that is quiet.
  bool alive() const;

  // True once a frame has arrived at all: the main board has answered, and the
  // panel is no longer alone on the wire.
  bool heard() const { return _received != 0; }

  static constexpr uint16_t PAYLOAD_MAX = 1024;
  static constexpr uint16_t FRAME_MAX = 2 + PAYLOAD_MAX + 2;

  // Frames the parser has assembled, called from the link task: the bytes are
  // that frame's, and are only valid for the call.
  void deliver(const uint8_t *payload, uint16_t length);

private:
  Link() = default;

  static void taskEntry(void *argument);
  void run();
  void parserReset();
  bool parserFeed(uint8_t byte);

  FrameHandler _handler = nullptr;
  void *_context = nullptr;

  // The payload being assembled, and where in it the parser is.
  uint8_t _payload[PAYLOAD_MAX] = {};
  uint16_t _length = 0;
  uint16_t _completed = 0;
  uint16_t _filled = 0;
  uint16_t _sum = 0;
  uint8_t _prefixBytes = 0;
  uint8_t _checksumLow = 0;
  uint8_t _checksumBytes = 0;
  uint8_t _state = 0;

  volatile uint32_t _received = 0;
  volatile uint32_t _refused = 0;
  volatile uint32_t _lastFrameMs = 0;
  volatile uint32_t _bytesRead = 0;
  bool _readyLine = false;
};

} // namespace ThetaGP::Drivers::Link
