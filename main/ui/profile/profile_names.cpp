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

#include "ui/profile/profile_names.h"

#include <cmath>

#include "ui/profile/profile_data.h"
#include "ui/spring.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Profile::Names {

using Theme::colour;
using Theme::fill;
using Theme::surface;
using Theme::text;

namespace {

// The frame the level has: the names along the arc and the rail beside them.
constexpr int kCardW = 156;
constexpr int kCardH = 38;
constexpr int kPitch = 40;
constexpr int kCenter = 70;
constexpr int kCurve = 22;
constexpr float kVisible = 1.35f;
constexpr int kRailX = 176;
constexpr int kRailY = 12;
constexpr int kRailH = 116;
constexpr int kThumbH = 20;

float s_scroll;
uint8_t s_count;
lv_obj_t *s_cards[Data::kNamesMax] = {};
lv_obj_t *s_titles[Data::kNamesMax] = {};
lv_obj_t *s_thumb;

void (*s_arrived)(uint8_t index);

// The wheel's position: the cards and the thumb are laid out for it at every
// frame.
void scrolled(void *key, float value) {
  (void)key;
  s_scroll = value;
  layout();
}

// Where the move came to rest is the card the mark stands on.
void settled(void *key) {
  (void)key;
  int row = (int)std::floor(s_scroll + 0.5f);
  if (row < 0) {
    row = 0;
  }
  if (row >= s_count) {
    row = s_count - 1;
  }
  s_scroll = (float)row;
  layout();
  if (s_arrived != nullptr) {
    s_arrived((uint8_t)row);
  }
}

float rel(int i) { return (float)i - s_scroll; }

} // namespace

void layout(void) {
  int chosen = (int)std::floor(s_scroll + 0.5f);
  chosen = ((chosen % s_count) + s_count) % s_count;

  for (int i = 0; i < s_count; ++i) {
    const float away = rel(i);
    const float far = away < 0 ? -away : away;
    lv_obj_t *card = s_cards[i];
    if (card == nullptr) {
      continue;
    }
    if (far > kVisible) {
      lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);

    const int bend = (int)(kCurve * away * away);
    lv_obj_set_pos(card, 6 - bend,
                   kCenter - kCardH / 2 + (int)(away * kPitch));
    lv_obj_set_style_opa(card, (lv_opa_t)(255 - (int)(95 * far)), 0);

    const bool on = i == chosen;
    fill(card, on ? Theme::SURFACE_HI : Theme::SURFACE);
    lv_obj_set_style_border_width(card, on ? 0 : 1, 0);
    lv_obj_set_style_text_color(s_titles[i],
                                colour(on ? Theme::TEXT : Theme::MUTED), 0);
  }

  if (s_thumb != nullptr) {
    float t = s_scroll / (float)(s_count > 1 ? s_count - 1 : 1);
    if (t < 0) {
      t = 0;
    }
    if (t > 1) {
      t = 1;
    }
    lv_obj_set_y(s_thumb, (int)(t * (kRailH - kThumbH)));
  }
}

void build(lv_obj_t *parent, uint8_t count, int at) {
  s_count = count;
  s_scroll = (float)at;

  for (int i = 0; i < s_count; ++i) {
    lv_obj_t *card = surface(parent, 0, 0, kCardW, kCardH);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    char name[26] = {};
    Data::nameLabel((uint8_t)i, name, sizeof name);
    s_titles[i] = text(card, name, &lv_font_montserrat_14, Theme::TEXT, 14, 11,
                       kCardW - 28);
    s_cards[i] = card;
  }

  lv_obj_t *track = lv_obj_create(parent);
  lv_obj_remove_style_all(track);
  lv_obj_set_pos(track, kRailX, kRailY);
  lv_obj_set_size(track, 2, kRailH);
  lv_obj_set_style_radius(track, 1, 0);
  fill(track, 0x3A2A1F);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

  s_thumb = lv_obj_create(track);
  lv_obj_remove_style_all(s_thumb);
  lv_obj_set_pos(s_thumb, 0, 0);
  lv_obj_set_size(s_thumb, 2, kThumbH);
  lv_obj_set_style_radius(s_thumb, 1, 0);
  fill(s_thumb, 0xFDBA74);

  layout();
}

void slide(int step, void (*arrived)(uint8_t index)) {
  s_arrived = arrived;
  Spring::run(&s_scroll, Spring::CARDS, s_scroll, s_scroll + (float)step,
              scrolled, settled);
}

void stop(void) { Spring::stop(&s_scroll); }

void drop(void) {
  for (int i = 0; i < Data::kNamesMax; ++i) {
    s_cards[i] = nullptr;
    s_titles[i] = nullptr;
  }
  s_thumb = nullptr;
}

} // namespace ThetaGP::Ui::Profile::Names
