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

#include "ui/profile/profile_popup.h"

#include "ui/profile/profile_data.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Profile::Popup {

using Theme::colour;
using Theme::fill;
using Theme::text;

namespace {

// The window is the page less twenty-five pixels at either side and ten at the
// top and the bottom.
constexpr int kWidth = 420 - 2 * 25;
constexpr int kHeight = 140 - 2 * 10;
constexpr int kX = 25;
constexpr int kY = 10;
constexpr int kPad = 12;
constexpr int kName = 26;
constexpr int kRow = 30;
constexpr int kStep = 22;
constexpr int kValue = 100;

lv_obj_t *s_window;
lv_obj_t *s_title;
lv_obj_t *s_row[Data::kRows];
lv_obj_t *s_name[Data::kRows];
lv_obj_t *s_mark[Data::kRows];

} // namespace

void open(lv_obj_t *parent) {
  if (s_window != NULL) {
    return;
  }

  s_window = lv_obj_create(parent);
  lv_obj_remove_style_all(s_window);
  lv_obj_set_pos(s_window, kX, kY);
  lv_obj_set_size(s_window, kWidth, kHeight);
  fill(s_window, Theme::SURFACE);
  lv_obj_set_style_radius(s_window, 10, 0);
  lv_obj_set_style_border_color(s_window, colour(Theme::BORDER), 0);
  lv_obj_set_style_border_width(s_window, 1, 0);
  lv_obj_remove_flag(s_window, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_window, LV_OBJ_FLAG_SCROLLABLE);

  s_title = text(s_window, "OPTIONS", &lv_font_montserrat_12, Theme::MUTED, kPad,
                 10, kWidth - 2 * kPad);
  for (int i = 0; i < Data::kRows; ++i) {
    s_mark[i] = text(s_window, "", &lv_font_montserrat_12, Theme::MUTED, kPad,
                     kRow + 1 + i * kStep, 10);
    s_name[i] = text(s_window, "", &lv_font_montserrat_12, Theme::MUTED, kName,
                     kRow + i * kStep, kWidth - kName - kPad - kValue);
    s_row[i] = text(s_window, "", &lv_font_montserrat_14, Theme::TEXT,
                    kWidth - kPad - kValue, kRow + i * kStep, kValue);
    lv_obj_set_style_text_align(s_row[i], LV_TEXT_ALIGN_RIGHT, 0);
  }
}

bool isOpen(void) { return s_window != NULL; }

void show(bool values, uint8_t setting, uint8_t valueRow) {
  if (s_window == NULL) {
    return;
  }

  lv_label_set_text(s_title,
                    values ? Data::settingName(setting) : "OPTIONS");

  char lines[Data::kRows][2][40] = {};
  Data::listBlock(values, setting, valueRow, lines, 0);

  for (int i = 0; i < Data::kRows; ++i) {
    const bool used = lines[i][1][0] != '\0';
    const bool on = used && i == (values ? valueRow : setting);

    // A list of settings carries the key at the left and the value it holds
    // at the right; a list of values has no key, so the value takes the line.
    lv_obj_set_x(s_row[i], values ? (lv_coord_t)kName
                                  : (lv_coord_t)(kWidth - kPad - kValue));
    lv_obj_set_width(s_row[i], values ? (lv_coord_t)(kWidth - kName - kPad)
                                      : kValue);
    lv_obj_set_style_text_align(
        s_row[i], values ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_row[i], used ? lines[i][1] : "");
    lv_obj_set_style_text_color(s_row[i],
                                colour(on ? Theme::ACCENT : Theme::TEXT), 0);

    lv_label_set_text(s_name[i], values ? "" : lines[i][0]);
    lv_obj_set_style_text_color(s_name[i],
                                colour(on ? Theme::ACCENT : Theme::MUTED), 0);

    lv_label_set_text(s_mark[i], on ? (values ? "*" : ">") : "");
    lv_obj_set_style_text_color(s_mark[i],
                                colour(on ? Theme::ACCENT : Theme::MUTED), 0);

    lv_obj_t *parts[3] = {s_row[i], s_name[i], s_mark[i]};
    for (lv_obj_t *part : parts) {
      if (used) {
        lv_obj_remove_flag(part, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

void foreground(void) {
  if (s_window != NULL) {
    lv_obj_move_foreground(s_window);
  }
}

void close(void) {
  if (s_window != NULL) {
    lv_obj_delete(s_window);
    s_window = NULL;
  }
}

} // namespace ThetaGP::Ui::Profile::Popup
