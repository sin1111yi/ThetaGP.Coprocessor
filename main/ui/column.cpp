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

#include "ui/column.h"

#include <cmath>

#include "ui/theme.h"

namespace ThetaGP::Ui::Cards {

using Theme::colour;
using Theme::text;

namespace {

constexpr int kPad = 5; // the text's own margin inside a card
constexpr int kRadius = 7;
constexpr uint32_t kStroke = 0x2A2D33;
constexpr uint32_t kLive = 0xF97316;
constexpr int kMoveMs = 480;

// A damped turn: the column starts slowly, runs through the middle and is
// settled well before the move is over.
int32_t damped(const lv_anim_t *anim) {
  const float t = (float)anim->act_time / (float)anim->duration;
  const float settle =
      1.0f - std::exp(-9.0f * t) *
                 (std::cos(11.0f * t) + 0.82f * std::sin(11.0f * t));
  return anim->start_value +
         (int32_t)((float)(anim->end_value - anim->start_value) * settle);
}

void settle(lv_anim_t *anim) {
  if (anim->var != nullptr) {
    lv_obj_set_y((lv_obj_t *)anim->var, 0);
  }
}

} // namespace

void Column::build(lv_obj_t *root, int x, int y, int width) {
  _window = lv_obj_create(root);
  lv_obj_remove_style_all(_window);
  lv_obj_set_pos(_window, (lv_coord_t)x, (lv_coord_t)y);
  lv_obj_set_size(_window, (lv_coord_t)width, (lv_coord_t)HEIGHT);
  lv_obj_set_style_bg_opa(_window, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(_window, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(_window, LV_OBJ_FLAG_SCROLLABLE);

  _list = lv_obj_create(_window);
  lv_obj_remove_style_all(_list);
  lv_obj_set_pos(_list, 0, 0);
  lv_obj_set_size(_list, (lv_coord_t)width, (lv_coord_t)HEIGHT);
  lv_obj_set_style_bg_opa(_list, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(_list, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(_list, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < ROWS; ++i) {
    lv_obj_t *card = lv_obj_create(_list);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, 0, (lv_coord_t)(i * PITCH));
    lv_obj_set_size(card, (lv_coord_t)width, CARD);
    lv_obj_set_style_radius(card, kRadius, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(card, colour(kStroke), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    _name[i] = text(card, "", &lv_font_montserrat_12, Theme::MUTED, kPad, 4,
                    width - kPad * 2 - 40);
    lv_obj_set_style_text_letter_space(_name[i], 1, 0);

    _value[i] = lv_label_create(card);
    lv_obj_set_style_text_font(_value[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_value[i], colour(Theme::TEXT), 0);
    lv_obj_set_width(_value[i], (lv_coord_t)(width - kPad * 2));
    lv_obj_set_style_text_align(_value[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(_value[i], LV_LABEL_LONG_CLIP);
    lv_obj_align(_value[i], LV_ALIGN_RIGHT_MID, -kPad, 0);

    _card[i] = card;
  }
}

void Column::drop(void) {
  _window = nullptr;
  _list = nullptr;
  for (int i = 0; i < ROWS; ++i) {
    _card[i] = nullptr;
    _name[i] = nullptr;
    _value[i] = nullptr;
  }
}

void Column::fill(const Line *lines, int count, int first) {
  for (int i = 0; i < ROWS; ++i) {
    if (_card[i] == nullptr) {
      continue;
    }
    const int at = first + i;
    if (lines == nullptr || at < 0 || at >= count) {
      lv_obj_add_flag(_card[i], LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(_card[i], LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_name[i], lines[at].label != nullptr ? lines[at].label : "");
    lv_label_set_text(_value[i],
                      lines[at].value != nullptr ? lines[at].value : "");
  }
}

void Column::focus(int row) {
  for (int i = 0; i < ROWS; ++i) {
    if (_card[i] == nullptr) {
      continue;
    }
    const bool on = i == row;
    lv_obj_set_style_border_color(_card[i], colour(on ? kLive : kStroke), 0);
    lv_obj_set_style_text_color(_name[i], colour(on ? kLive : Theme::MUTED), 0);
    lv_obj_set_style_text_color(_value[i], colour(on ? kLive : Theme::TEXT), 0);
  }
}

void Column::rest(void) {
  if (_list != nullptr) {
    lv_anim_delete(_list, nullptr);
    lv_obj_set_y(_list, 0);
  }
}

void Column::slide(int direction) {
  if (_list == nullptr) {
    return;
  }

  const int from = direction > 0 ? PITCH : -PITCH;
  lv_anim_delete(_list, nullptr);
  lv_obj_set_y(_list, (lv_coord_t)from);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, _list);
  lv_anim_set_values(&anim, from, 0);
  lv_anim_set_exec_cb(&anim, [](void *var, int32_t value) {
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)value);
  });
  lv_anim_set_path_cb(&anim, damped);
  lv_anim_set_duration(&anim, kMoveMs);
  lv_anim_set_completed_cb(&anim, settle);
  lv_anim_start(&anim);
}

} // namespace ThetaGP::Ui::Cards
