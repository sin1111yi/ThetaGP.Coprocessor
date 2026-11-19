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

// The board's one button, read as a line: a short press and a long one are
// told apart by how long the line was held low, and are reported once, when it
// is let go. The driver knows nothing of what a press means.

#pragma once

#include <cstdint>

namespace ThetaGP::Drivers::Input {

enum class Press : uint8_t { None, Short, Long };

class Button {
public:
  static Button &getInstance() {
    static Button instance;
    return instance;
  }

  // Claims the pin, pulled up: the line reads low while the button is held.
  void init();

  // Samples the line at `nowMs`. Returns the press that was let go just now,
  // None on every other call.
  Press poll(uint32_t nowMs);

private:
  Button() = default;

  bool _down = false;
  uint32_t _pressedAt = 0;
};

} // namespace ThetaGP::Drivers::Input
