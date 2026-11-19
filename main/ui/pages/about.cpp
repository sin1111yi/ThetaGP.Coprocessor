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

#include "build_info.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Pages {

using Theme::text;

void drawAbout(lv_obj_t *parent, uint8_t selected) {
  (void)selected;
  char version[40];
  snprintf(version, sizeof version, "firmware %s", BuildInfo::VERSION);

  text(parent, "ThetaGP coprocessor", &lv_font_montserrat_14, Theme::ACCENT, 10,
       21, 400);
  text(parent, version, &lv_font_montserrat_12, Theme::TEXT, 10, 43, 400);
  text(parent, "ESP32-S3 + NV3007", &lv_font_montserrat_12, Theme::MUTED, 10,
       63, 400);
  text(parent, "schema ThetaGP.PB", &lv_font_montserrat_12, Theme::MUTED, 10,
       81, 400);
}

} // namespace ThetaGP::Ui::Pages
