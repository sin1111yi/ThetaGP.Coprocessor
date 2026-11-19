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
using Theme::text;

void drawItems(lv_obj_t *parent, const Item *items, uint8_t count,
               uint8_t selected) {
  for (uint8_t i = 0; i < count; ++i) {
    const bool chosen = i == selected;
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, 8, i * 18);
    lv_obj_set_size(row, 404, 17);
    lv_obj_set_style_radius(row, 5, 0);
    if (chosen) {
      fill(row, Theme::SURFACE_HI);
    } else {
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    }

    if (chosen) {
      lv_obj_t *bar = lv_obj_create(row);
      lv_obj_remove_style_all(bar);
      lv_obj_set_pos(bar, 2, 2);
      lv_obj_set_size(bar, 3, 13);
      lv_obj_set_style_radius(bar, 2, 0);
      lv_obj_set_style_bg_color(bar, colour(Theme::ACCENT), 0);
      lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    }

    text(row, items[i].label, &lv_font_montserrat_14,
         chosen ? Theme::TEXT : Theme::MUTED, 12, 0, 340);
    text(row, ">", &lv_font_montserrat_14, chosen ? Theme::ACCENT : Theme::MUTED,
         384, 0, 16);
  }
}

} // namespace ThetaGP::Ui::Pages
