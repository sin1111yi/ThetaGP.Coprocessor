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

#include "drivers/display/lvgl_display.h"

#include <cstdio>

#include "conf/board.h"
#include "drivers/display/nv3007_panel.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"

namespace ThetaGP::Drivers::Display {

uint32_t LvglDisplay::tickMs() {
  return (uint32_t)(esp_timer_get_time() / 1000);
}

void LvglDisplay::init() {
  lv_init();
  lv_tick_set_cb(tickMs);

  // The panel is 156x428 native and mounted across the board; the drawing
  // surface is the quarter turn that puts it upright.
  _display =
      lv_nv3007_create(LCD_H_RES, LCD_V_RES, LV_LCD_FLAG_NONE,
                       Nv3007Panel::sendCommand, Nv3007Panel::sendColour);
  if (_display == nullptr) {
    printf("nv3007: no display\n");
    return;
  }
  lv_display_set_rotation(_display, LV_DISPLAY_ROTATION_270);

  // Twenty lines per buffer: enough that a page turn is a handful of flushes,
  // and small enough to leave the rest of RAM to LVGL's pool.
  const size_t pixels = LCD_V_RES * 20;
  uint8_t *first = static_cast<uint8_t *>(
      heap_caps_malloc(pixels * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  uint8_t *second = static_cast<uint8_t *>(
      heap_caps_malloc(pixels * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (first == nullptr || second == nullptr) {
    printf("nv3007: no draw buffer\n");
    _display = nullptr;
    return;
  }
  lv_display_set_buffers(_display, first, second, pixels * 2,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  printf("nv3007: %dx%d ready\n", LCD_H_RES, LCD_V_RES);
}

} // namespace ThetaGP::Drivers::Display
