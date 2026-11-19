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

#include "drivers/input/button.h"

#include "conf/board.h"

#include "driver/gpio.h"

namespace ThetaGP::Drivers::Input {

namespace {

// Held this long, a press opens; shorter, it moves the mark.
constexpr uint32_t kLongPressMs = 500;

} // namespace

void Button::init() {
  gpio_config_t button = {};
  button.pin_bit_mask = 1ULL << PIN_BUTTON;
  button.mode = GPIO_MODE_INPUT;
  button.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&button);
}

Press Button::poll(uint32_t nowMs) {
  Press press = Press::None;
  const bool held = gpio_get_level(PIN_BUTTON) == 0;
  if (held && !_down) {
    _pressedAt = nowMs;
  } else if (!held && _down) {
    press = nowMs - _pressedAt >= kLongPressMs ? Press::Long : Press::Short;
  }
  _down = held;
  return press;
}

} // namespace ThetaGP::Drivers::Input
