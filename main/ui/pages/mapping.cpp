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
using Theme::fill;
using Theme::surface;

void drawMapping(lv_obj_t *parent, uint8_t selected) {
  static const char *keys[16] = {"B1", "B2", "B3", "B4", "L1", "R1",
                                 "L2", "R2", "S1", "S2", "L3", "R3",
                                 "A1", "A2", "UP", "DN"};
  const int cell_w = 30;
  const int cell_h = 28;
  const int gap = 3;

  for (int i = 0; i < 16; ++i) {
    const int col = i % 8;
    const int row = i / 8;
    lv_obj_t *key = surface(parent, 6 + col * (cell_w + gap),
                            28 + row * (cell_h + gap), cell_w, cell_h);
    lv_obj_set_style_radius(key, 5, 0);
    const bool on_cursor = i == selected;
    if (on_cursor) {
      fill(key, Theme::SURFACE_HI);
      lv_obj_set_style_border_width(key, 0, 0);
    }

    lv_obj_t *name = lv_label_create(key);
    lv_label_set_text(name, keys[i]);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(
        name, colour(on_cursor ? Theme::ACCENT : Theme::TEXT), 0);
    lv_obj_center(name);
  }
}

} // namespace ThetaGP::Ui::Pages
