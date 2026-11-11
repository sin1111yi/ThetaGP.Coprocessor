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

// The NV3007 panel on its SPI bus: the pins it answers on, the reset sequence
// it needs and the two writes LVGL makes — a command with its parameters, and
// a window of colour.

#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/spi_master.h"
#include "lvgl.h"

namespace ThetaGP::Drivers::Display {

class Nv3007Panel {
public:
  static Nv3007Panel &getInstance() {
    static Nv3007Panel instance;
    return instance;
  }

  // Claims the pins and the bus, then resets the panel into a known state. The
  // backlight stays off until the screen has something to show.
  void init();

  // The panel's backlight, off until the first frame is on the glass.
  void backlight(bool on);
  [[nodiscard]] bool backlightOn() const { return _backlightOn; }

  // The callbacks LVGL's NV3007 driver writes through.
  static void sendCommand(lv_display_t *disp, const uint8_t *cmd,
                          size_t cmdSize, const uint8_t *param,
                          size_t paramSize);
  static void sendColour(lv_display_t *disp, const uint8_t *cmd, size_t cmdSize,
                         uint8_t *param, size_t paramSize);

private:
  void reset();

  // The panel takes RGB565 most significant byte first; LVGL's buffer is the
  // other way round.
  static void swapRgb565(uint8_t *bytes, size_t length);

  spi_device_handle_t _spi = nullptr;
  bool _backlightOn = false;
};

} // namespace ThetaGP::Drivers::Display
