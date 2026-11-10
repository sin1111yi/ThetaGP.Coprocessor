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

// LVGL on this board: its clock, the display object the panel is wrapped in,
// and the two draw buffers the renderer fills. The flush is synchronous
// because a frame here is a few hundred pixels.

#pragma once

#include "lvgl.h"

namespace ThetaGP::Drivers::Display {

class LvglDisplay {
public:
  static LvglDisplay &getInstance() {
    static LvglDisplay instance;
    return instance;
  }

  void init();

  lv_display_t *handle() const { return _display; }

private:
  static uint32_t tickMs();

  lv_display_t *_display = nullptr;
};

} // namespace ThetaGP::Drivers::Display
