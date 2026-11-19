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

#include <cstdio>

#include "ui/model.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Pages {

using Theme::colour;
using Theme::surface;
using Theme::text;

void drawMonitor(lv_obj_t *parent, uint8_t selected) {
  (void)selected;
  const Model &m = model();

  lv_obj_t *profile = surface(parent, 8, 23, 198, 70);
  text(profile, "PROFILE", &lv_font_montserrat_12, Theme::MUTED, 10, 6, 90);
  text(profile, m.profile_name, &lv_font_montserrat_20, Theme::TEXT, 10, 22,
       170);

  char body[32];
  snprintf(body, sizeof body, "profile %u of %u", (unsigned)m.profile_id,
           (unsigned)m.profile_count);
  text(profile, body, &lv_font_montserrat_12, Theme::MUTED, 10, 48, 170);

  lv_obj_t *link = surface(parent, 214, 23, 198, 70);
  text(link, "LINK", &lv_font_montserrat_12, Theme::MUTED, 10, 6, 90);

  lv_obj_t *dot = lv_obj_create(link);
  lv_obj_remove_style_all(dot);
  lv_obj_set_pos(dot, 10, 26);
  lv_obj_set_size(dot, 8, 8);
  lv_obj_set_style_radius(dot, 4, 0);
  lv_obj_set_style_bg_color(dot, colour(m.link ? Theme::ACCENT : Theme::MUTED),
                            0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

  text(link, m.link ? "live" : "waiting", &lv_font_montserrat_14,
       m.link ? Theme::TEXT : Theme::MUTED, 24, 21, 80);

  char rate[24];
  if (m.report_hz % 1000 == 0) {
    snprintf(rate, sizeof rate, "%u kHz", (unsigned)(m.report_hz / 1000));
  } else {
    snprintf(rate, sizeof rate, "%u Hz", (unsigned)m.report_hz);
  }
  text(link, rate, &lv_font_montserrat_14, Theme::ACCENT, 10, 46, 170);
}

} // namespace ThetaGP::Ui::Pages
