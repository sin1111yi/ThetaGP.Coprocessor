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

// The panel's side of Link.proto: it greets the board, says what the screen
// shows on its cadence, and applies what the board asks for on the task that
// owns the widgets. What it speaks over and what it reports are handed in, so
// this layer names no driver. The arms themselves are read in dispatch.cpp.

#pragma once

#include <cstdint>

#include "Link.pb.h"

#include "protocol/transport.h"
#include "wire/dispatch.h"

namespace ThetaGP::Protocol {

class PanelProtocol {
public:
  static PanelProtocol &getInstance() {
    static PanelProtocol instance;
    return instance;
  }

  // Takes what it speaks over and what it reports, and says hello for the
  // first time.
  void init(const Transport &transport, const Panel &panel);

  // Says hello while the board has not answered, beats on its cadence, and
  // beats at once when what the screen shows has changed.
  void tick();

  // One payload the wire assembled: read on the task that takes frames, and
  // applied on the next tick.
  void feed(const uint8_t *payload, uint16_t length);

private:
  PanelProtocol() = default;

  // What the last frames asked for: applied on the task that owns the widgets,
  // and only what is still pending when `tick` runs.
  void apply();

  // One message, encoded into the payload the transport carries.
  bool send(const ThetaGP_Link_Message &message);

  void sendHello();
  void beat(bool now);

  // What the last beat said, packed: the beat is sent on a change of this and
  // nothing else.
  uint32_t shape() const;

  Transport _transport;
  Panel _panel;
  Wire::Inbox _inbox;

  uint32_t _lastHelloMs = 0;
  uint32_t _lastBeatMs = 0;
  uint32_t _lastShape = 0;
};

} // namespace ThetaGP::Protocol
