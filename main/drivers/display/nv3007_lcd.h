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

// The NV3007 as an esp_lcd panel: the interface the LVGL adapter registers,
// carrying the panel's own power-up sequence and its window-and-write.

#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

namespace ThetaGP::Drivers::Display {

class Nv3007Lcd {
public:
  static Nv3007Lcd &getInstance() {
    static Nv3007Lcd instance;
    return instance;
  }

  // Claims the pins and the bus, sends the power-up sequence, and leaves the
  // panel ready for its first window. The backlight stays off.
  void init();
  void backlight(bool on);
  [[nodiscard]] bool backlightOn() const { return _backlightOn; }

  esp_lcd_panel_handle_t panel() const { return _panel; }
  esp_lcd_panel_io_handle_t io() const { return _io; }

private:
  Nv3007Lcd() = default;

  esp_lcd_panel_io_handle_t _io = nullptr;
  esp_lcd_panel_handle_t _panel = nullptr;
  bool _backlightOn = false;
};

} // namespace ThetaGP::Drivers::Display
