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

// What the messages travel on, and what they report. The session is handed
// both, so the message layer names no driver: the board fills these in.

#pragma once

#include <cstdint>

namespace ThetaGP::Protocol {

// The wire the payloads go out on, and what it has seen: the frames it read
// and the ones it refused.
struct Transport {
  bool (*send)(const uint8_t *payload, uint16_t length, void *context);
  bool (*heard)(void *context);
  uint32_t (*refused)(void *context);
  void *context = nullptr;
};

// The panel the beat reports: the surface the menu draws on, and the light
// behind it.
struct Panel {
  uint16_t (*width)(void *context);
  uint16_t (*height)(void *context);
  bool (*backlightOn)(void *context);
  void *context = nullptr;
};

} // namespace ThetaGP::Protocol
