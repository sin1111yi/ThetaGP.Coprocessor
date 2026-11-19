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

// The NV3007's power-up sequence: what the panel has to be told, in order,
// before it will take a window. The table is the panel's own and has nothing
// of the bus in it; the driver walks it.

#pragma once

#include <cstddef>
#include <cstdint>

namespace ThetaGP::Drivers::Display::Nv3007 {

// One step of the power-up: a command with its parameters, or, when delayMs
// is not zero, a wait the panel asks for before the next one.
struct Step {
  uint8_t reg;
  uint8_t count;
  uint8_t data[16];
  uint16_t delayMs;
};

extern const Step INIT[];
extern const size_t INIT_COUNT;

} // namespace ThetaGP::Drivers::Display::Nv3007
