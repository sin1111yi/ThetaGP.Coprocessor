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

// The palette every page draws from, and the three shapes they are built out
// of. A widget is one flat fill with one hairline border: a frame costs a copy,
// never a blend.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ThetaGP::Ui::Theme {

inline constexpr uint32_t BG = 0x0C0D10;
inline constexpr uint32_t SURFACE = 0x17191E;
inline constexpr uint32_t SURFACE_HI = 0x21242A;
inline constexpr uint32_t BORDER = 0x2A2D33;
inline constexpr uint32_t TEXT = 0xF2F4F6;
inline constexpr uint32_t MUTED = 0x8A8F98;
inline constexpr uint32_t ACCENT = 0xF97316;

inline lv_color_t colour(uint32_t rgb) { return lv_color_hex(rgb); }

inline void fill(lv_obj_t *obj, uint32_t rgb) {
  lv_obj_set_style_bg_color(obj, colour(rgb), 0);
  lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

// An empty rounded panel: the shape a page is built from.
inline lv_obj_t *surface(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  fill(obj, SURFACE);
  lv_obj_set_style_radius(obj, 8, 0);
  lv_obj_set_style_border_color(obj, colour(BORDER), 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  return obj;
}

// A single clipped line at a fixed place, which is all a row ever holds.
inline lv_obj_t *text(lv_obj_t *parent, const char *value,
                      const lv_font_t *font, uint32_t rgb, int x, int y,
                      int w) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, value);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, colour(rgb), 0);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, w);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  return label;
}

} // namespace ThetaGP::Ui::Theme
