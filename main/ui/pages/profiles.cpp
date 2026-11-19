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

#include "ui/model.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Pages {

using Theme::fill;
using Theme::surface;
using Theme::text;

namespace {

const char *const kNames[3] = {"Main", "Racing", "Retro"};

} // namespace

const char *profileName(uint8_t index) {
  return index < 3 ? kNames[index] : kNames[0];
}

void drawProfiles(lv_obj_t *parent, uint8_t selected) {
  const Model &m = model();
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *row = surface(parent, 8, 23 + i * 24, 404, 21);
    lv_obj_set_style_radius(row, 5, 0);
    if ((uint8_t)i == selected) {
      fill(row, Theme::SURFACE_HI);
      lv_obj_set_style_border_width(row, 0, 0);
    } else {
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(row, 0, 0);
    }

    const bool active = (uint32_t)(i + 1) == m.profile_id;
    text(row, kNames[i], &lv_font_montserrat_14,
         active ? Theme::ACCENT : Theme::TEXT, 10, 1, 300);
  }
}

} // namespace ThetaGP::Ui::Pages
