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

#include "ui/pages/pages.h"

#include "ui/theme.h"

namespace ThetaGP::Ui::Pages {

using Theme::colour;
using Theme::surface;
using Theme::text;

void drawOptions(lv_obj_t *parent, uint8_t selected) {
  static const char *keys[4] = {"usb.input_mode", "led.bri", "map.socd",
                                "usb.wired_report_hz"};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *row = surface(parent, 8, 15 + i * 22, 404, 19);
    lv_obj_set_style_radius(row, 5, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    if ((uint8_t)i == selected) {
      Theme::fill(row, Theme::SURFACE_HI);
    } else {
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    }
    text(row, keys[i], &lv_font_montserrat_12,
         (uint8_t)i == selected ? Theme::TEXT : Theme::MUTED, 10, 1, 300);
  }
}

} // namespace ThetaGP::Ui::Pages
