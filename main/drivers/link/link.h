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

// The wire to the main board: one UART, the frames the wire layer assembles,
// and the ready line the board waits on. The bytes are read on this task; the
// frames are taken on the task that owns the widgets, so the two cores meet in
// the frame queue and never in a call.
//
// A driver and nothing else: no message is named here, and no widget.

#pragma once

#include <cstdint>

#include "wire/frame.h"

namespace ThetaGP::Drivers::Link {

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

  // Takes the oldest whole frame to arrive, on the task above. The bytes are
  // that slot's until it is handed out again.
  bool take(Wire::Frame::Payload &out);

  // Frames taken and refused since boot, as the status page reads them.
  uint32_t received(void) const { return _taken; }
  uint32_t refused(void) const;

  // True while frames are still arriving: a beat that stopped a second and a
  // half ago is a link that is down, not one that is quiet.
  bool alive(void) const;

  // True once a frame has arrived at all: the main board has answered, and the
  // panel is no longer alone on the wire.
  bool heard(void) const { return _taken != 0; }

  static constexpr uint16_t PAYLOAD_MAX = Wire::Frame::PAYLOAD_MAX;
  static constexpr uint16_t FRAME_MAX = Wire::Frame::FRAME_MAX;

private:
  Link() = default;

  static void taskEntry(void *argument);
  void run();

  Wire::Frame _frame;

  volatile uint32_t _taken = 0;
  volatile uint32_t _lastFrameMs = 0;
  volatile uint32_t _bytesRead = 0;
  bool _readyLine = false;
};

} // namespace ThetaGP::Drivers::Link
