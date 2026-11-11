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

// What the panel says to the main board, and what it does with what comes
// back: the messages of Link.proto on top of the bytes the transport carries.
// The two greetings name the boards, the main board's state fills the pages,
// the panel's beat says what is on screen, and a nav step moves it.

#pragma once

#include <cstdint>

#include "link.h"

namespace ThetaGP::Drivers::Link {

class PanelProtocol {
public:
  static PanelProtocol &getInstance() {
    static PanelProtocol instance;
    return instance;
  }

  // Binds the transport's handler and says hello for the first time.
  void init();

  // Says hello while the board has not answered, beats on its cadence, and
  // beats at once when what the screen shows has changed.
  void tick();

private:
  PanelProtocol() = default;

  static void onFrame(const uint8_t *payload, uint16_t length, void *context);
  void handle(const uint8_t *payload, uint16_t length);

  // What the link carried is left for `tick`: the frame arrives on the link's
  // own task, and only the task that drives LVGL touches the widgets.
  void apply();

  void sendHello();
  void beat(bool now);

  // What the last beat said, packed: the beat is sent on a change of this and
  // nothing else.
  uint32_t shape() const;

  // What the last frame asked for, waiting for the task that owns the widgets.
  volatile bool _boardPending = false;
  char _boardId[21] = {};
  char _boardRelease[21] = {};

  volatile bool _statePending = false;
  uint32_t _stateHz = 0;
  uint32_t _stateId = 0;
  uint32_t _stateCount = 0;
  char _stateName[25] = {};

  static constexpr uint8_t kNavQueue = 8;
  uint8_t _navAction[kNavQueue] = {};
  uint8_t _navPage[kNavQueue] = {};
  volatile uint8_t _navHead = 0;
  volatile uint8_t _navTail = 0;

  bool _greeted = false;
  uint32_t _lastHelloMs = 0;
  uint32_t _lastBeatMs = 0;
  uint32_t _lastShape = 0;
};

} // namespace ThetaGP::Drivers::Link
