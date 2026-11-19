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

// The arms of Link.proto read in one place: a payload goes in, and what it
// asked for is left in an inbox for the task that owns the widgets. The frame
// the message arrived in is the wire's; this layer only reads payloads, and it
// reads them on the task that applies them.

#pragma once

#include <cstdint>

namespace ThetaGP::Wire {

// What the board has asked for since the last time it was applied. Filled and
// emptied on the task that owns the widgets, so nothing here is volatile.
struct Inbox {
  bool boardPending = false;
  char boardId[21] = {};
  char release[21] = {};

  bool statePending = false;
  uint32_t hz = 0;
  uint32_t profileId = 0;
  uint32_t profileCount = 0;
  char profileName[25] = {};

  // A burst is applied in one go; the steps past this one are dropped, the
  // same way a full queue drops what it cannot hold.
  static constexpr uint8_t kNavMax = 8;
  uint8_t nav[kNavMax] = {};
  uint8_t navCount = 0;
};

// The arms of the schema, dispatched by the message the payload carries. A new
// arm is added here and nowhere else.
class Dispatch {
public:
  // Reads one payload and puts what it asks for in `inbox`. False for a
  // payload that is not one of ours: a build that speaks another schema, or
  // noise the checksum let through.
  static bool take(const uint8_t *payload, uint16_t length, Inbox &inbox);
};

} // namespace ThetaGP::Wire
