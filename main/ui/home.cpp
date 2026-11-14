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

#include "ui/home.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "build_info.h"
#include "ui/menu.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Home {

using Theme::colour;
using Theme::fill;
using Theme::surface;
using Theme::text;

// The page is three columns: names on the left, a rail in the middle, the
// chosen page's parameters on the right, and the link's names at the foot of
// the right column. The names travel along an arc as they move and wrap around
// at either end; the rail and the link line stay where they are.
//
// Every column is centred on the page's own middle, so the space above the top
// card and below the bottom card is the same, and so is the rail's.
#define CARD_COUNT  5
#define HOME_CARD_W 156
#define HOME_CARD_H 38
#define HOME_PITCH  40
#define HOME_CENTER 70
#define HOME_CURVE  22
#define HOME_VIS    1.35f

// The bar's amber thumb and the dark rail it runs on.
#define RAIL_X      176
#define RAIL_Y      12
#define RAIL_H      116
#define THUMB_H     20
#define THUMB       0xFDBA74
#define RAIL        0x3A2A1F

// The parameters fill the right column; the link line sits under them, its
// right edge on the same mark as their values.
#define PARAM_X     200
#define PARAM_W     214
// The right column shows four lines and rolls between them the way the link
// line rolls: a whole line, damped, with the next block waiting below.
#define PARAM_LINE  18
#define PARAM_ROWS  6
#define PARAM_H     (PARAM_LINE * PARAM_ROWS)
#define PARAM_Y     ((140 - PARAM_H) / 2)

static const char *const ICONS[CARD_COUNT] = {
    LV_SYMBOL_EYE_OPEN, LV_SYMBOL_SETTINGS, LV_SYMBOL_KEYBOARD,
    LV_SYMBOL_DIRECTORY, LV_SYMBOL_FILE};

static const char *const PARAM_KEYS[CARD_COUNT][PARAM_ROWS] = {
    {"LINK", "RATE", "PROFILE", "PEER", "FIRMWARE", "PANEL"},
    {"INPUT", "REPORT", "SAVE", "MODE", "SOURCE", "DEFAULT"},
    {"KEYS", "LAYER", "MODE", "DEFAULT", "SCHEME", "RESET"},
    {"ID", "COUNT", "NAME", "STATE", "MAPPING", "OPTIONS"},
    {"BOARD", "MCU", "FW", "PANEL", "BUILD", "PROTOCOL"},
};

static float s_scroll;
static float s_anim_from;
static float s_anim_diff;
static lv_obj_t *s_cards[CARD_COUNT];
static lv_obj_t *s_titles[CARD_COUNT];
static lv_obj_t *s_icons[CARD_COUNT];
static lv_obj_t *s_params;       // the window the strip is seen through
static lv_obj_t *s_paramsCanvas; // the two blocks that roll under it
static uint8_t *s_paramsPixels;
static lv_obj_t *s_thumb;

// ── the parameters column ──

// The right column reads the model, so the menu says what the pages say.
static void params(uint8_t item, char out[PARAM_ROWS][2][26]) {
  const Model &m = model();
  for (uint8_t row = 0; row < PARAM_ROWS; ++row) {
    snprintf(out[row][0], 26, "%s", PARAM_KEYS[item][row]);
    out[row][1][0] = '\0';
  }

  switch (item) {
  case 0:
    snprintf(out[0][1], 26, "%s", m.link ? "live" : "waiting");
    snprintf(out[1][1], 26, "%u Hz", (unsigned)m.report_hz);
    snprintf(out[2][1], 26, "%s %u/%u", m.profile_name, (unsigned)m.profile_id,
             (unsigned)m.profile_count);
    snprintf(out[3][1], 26, "%s", m.peer_id[0] != '\0' ? m.peer_id : "-");
    snprintf(out[4][1], 26, "%s",
             m.peer_version[0] != '\0' ? m.peer_version : "-");
    snprintf(out[5][1], 26, "%s", "428x156");
    break;
  case 1:
    snprintf(out[0][1], 26, "button");
    snprintf(out[1][1], 26, "%u Hz", (unsigned)m.report_hz);
    snprintf(out[2][1], 26, "auto");
    snprintf(out[3][1], 26, "usb");
    snprintf(out[4][1], 26, "%s", "config");
    snprintf(out[5][1], 26, "%s", "-");
    break;
  case 2:
    snprintf(out[0][1], 26, "16");
    snprintf(out[1][1], 26, "1");
    snprintf(out[2][1], 26, "hold");
    snprintf(out[3][1], 26, "-");
    snprintf(out[4][1], 26, "%s", "-");
    snprintf(out[5][1], 26, "%s", "-");
    break;
  case 3:
    snprintf(out[0][1], 26, "%u", (unsigned)m.profile_id);
    snprintf(out[1][1], 26, "%u", (unsigned)m.profile_count);
    snprintf(out[2][1], 26, "%s", m.profile_name);
    snprintf(out[3][1], 26, "%s", "active");
    snprintf(out[4][1], 26, "%s", "-");
    snprintf(out[5][1], 26, "%s", "-");
    break;
  default:
    snprintf(out[0][1], 26, "%s", m.board);
    snprintf(out[1][1], 26, "ESP32-S3");
    snprintf(out[2][1], 26, "%s", BuildInfo::VERSION);
    snprintf(out[3][1], 26, "428x156");
    snprintf(out[4][1], 26, "%s", "-");
    snprintf(out[5][1], 26, "%s", "1");
    break;
  }
}

// Paints two blocks into the strip: `first` at the top and `second` under it,
// so a roll slides one whole block and no frame is ever blank.
static void params_paint(const char first[PARAM_ROWS][2][26],
                         const char second[PARAM_ROWS][2][26]) {
  if (s_paramsCanvas == NULL) {
    return;
  }

  lv_canvas_fill_bg(s_paramsCanvas, colour(Theme::BG), LV_OPA_COVER);

  lv_layer_t layer;
  lv_canvas_init_layer(s_paramsCanvas, &layer);

  for (uint8_t pass = 0; pass < 2; ++pass) {
    const char (*rows)[2][26] = pass == 0 ? first : second;
    for (uint8_t i = 0; i < PARAM_ROWS; ++i) {
      const lv_coord_t top = (lv_coord_t)(pass * PARAM_H + i * PARAM_LINE);

      // The hairline above each line, as the column has always carried.
      lv_draw_rect_dsc_t rule;
      lv_draw_rect_dsc_init(&rule);
      rule.bg_color = colour(Theme::BORDER);
      rule.bg_opa = LV_OPA_COVER;
      lv_area_t ruleArea = {0, top, (lv_coord_t)(PARAM_W - 1), top};
      lv_draw_rect(&layer, &rule, &ruleArea);

      lv_draw_label_dsc_t label;
      lv_draw_label_dsc_init(&label);
      label.font = &lv_font_montserrat_12;
      label.color = colour(Theme::MUTED);
      label.text = rows[i][0];
      label.text_length = (uint32_t)std::strlen(rows[i][0]);
      label.align = LV_TEXT_ALIGN_LEFT;
      lv_area_t labelArea = {0, (lv_coord_t)(top + 2),
                             (lv_coord_t)(PARAM_W - 1), (lv_coord_t)(top + 15)};
      lv_draw_label(&layer, &label, &labelArea);

      lv_draw_label_dsc_t value;
      lv_draw_label_dsc_init(&value);
      value.font = &lv_font_montserrat_14;
      value.color = colour(Theme::TEXT);
      value.text = rows[i][1];
      value.text_length = (uint32_t)std::strlen(rows[i][1]);
      value.align = LV_TEXT_ALIGN_RIGHT;
      lv_area_t valueArea = {0, (lv_coord_t)(top + 1),
                             (lv_coord_t)(PARAM_W - 1), (lv_coord_t)(top + 17)};
      lv_draw_label(&layer, &value, &valueArea);
    }
  }

  lv_canvas_finish_layer(s_paramsCanvas, &layer);
}

// Puts the strip at rest on the card the mark is on.
static void params_show(void) {
  char block[PARAM_ROWS][2][26] = {};
  params(selected(), block);
  params_paint(block, block);
  if (s_paramsCanvas != NULL) {
    lv_obj_set_y(s_paramsCanvas, 0);
  }
}

static void params_roll_done(lv_anim_t *anim) {
  (void)anim;
  params_show();
}

// Rolls the strip one whole block, in the direction the mark moved.
// A damped turn: the block starts slowly, runs through the middle and is
// settled well before the move is over.
static int32_t damped_path(const lv_anim_t *anim) {
  const float t = (float)anim->act_time / (float)anim->duration;
  const float settle =
      1.0f -
      std::exp(-9.0f * t) * (std::cos(11.0f * t) + 0.82f * std::sin(11.0f * t));
  return anim->start_value +
         (int32_t)((float)(anim->end_value - anim->start_value) * settle);
}

static void params_roll(int step) {
  if (s_paramsCanvas == NULL) {
    return;
  }

  const int from = selected();
  const int to = ((from + step) % CARD_COUNT + CARD_COUNT) % CARD_COUNT;

  char leaving[PARAM_ROWS][2][26] = {};
  char arriving[PARAM_ROWS][2][26] = {};
  params((uint8_t)from, leaving);
  params((uint8_t)to, arriving);

  int fromY = 0;
  int toY = 0;
  if (step > 0) {
    params_paint(leaving, arriving);
    fromY = 0;
    toY = -PARAM_H;
  } else {
    params_paint(arriving, leaving);
    fromY = -PARAM_H;
    toY = 0;
  }

  lv_obj_set_y(s_paramsCanvas, fromY);
  lv_anim_delete(s_paramsCanvas, NULL);
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, s_paramsCanvas);
  lv_anim_set_values(&anim, fromY, toY);
  lv_anim_set_exec_cb(&anim, [](void *var, int32_t value) {
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)value);
  });
  lv_anim_set_path_cb(&anim, damped_path);
  lv_anim_set_duration(&anim, 480);
  lv_anim_set_completed_cb(&anim, params_roll_done);
  lv_anim_start(&anim);
}

// The shortest distance from item i to the mark, so a step over either end
// travels one place rather than the whole column.
static float rel(int i) {
  float away = (float)i - s_scroll;
  while (away > CARD_COUNT / 2.0f) {
    away -= CARD_COUNT;
  }
  while (away < -CARD_COUNT / 2.0f) {
    away += CARD_COUNT;
  }
  return away;
}

int selected(void) {
  int index = (int)std::floor(s_scroll + 0.5f) % CARD_COUNT;
  if (index < 0) {
    index += CARD_COUNT;
  }
  return index;
}

void layout(void) {
  const int chosen = selected();

  for (int i = 0; i < CARD_COUNT; ++i) {
    const float away = rel(i);
    const float far = away < 0 ? -away : away;
    lv_obj_t *card = s_cards[i];

    if (far > HOME_VIS) {
      lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);

    const int bend = (int)(HOME_CURVE * away * away);
    lv_obj_set_pos(card, 6 - bend,
                   HOME_CENTER - HOME_CARD_H / 2 + (int)(away * HOME_PITCH));
    lv_obj_set_style_opa(card, (lv_opa_t)(255 - (int)(95 * far)), 0);

    const bool on = i == chosen;
    fill(card, on ? Theme::SURFACE_HI : Theme::SURFACE);
    lv_obj_set_style_border_width(card, on ? 0 : 1, 0);
    lv_obj_set_style_text_color(s_titles[i],
                                colour(on ? Theme::TEXT : Theme::MUTED), 0);
    lv_obj_set_style_text_color(s_icons[i],
                                colour(on ? Theme::ACCENT : Theme::MUTED), 0);
  }

  // The thumb stops at the ends of the rail: the spring is for the cards, not
  // for running past the first or last place.
  float t = s_scroll / (float)(CARD_COUNT - 1);
  if (t < 0) {
    t = 0;
  }
  if (t > 1) {
    t = 1;
  }
  lv_obj_set_y(s_thumb, (int)(t * (RAIL_H - THUMB_H)));
}

// ── the page ──

void build(lv_obj_t *root) {
  s_scroll = (float)selected();

  // The cards read as labels rather than as sentences: upper case, and the
  // words standing clear of the icon.
  static const char *const TITLES[CARD_COUNT] = {
      "MONITOR", "OPTIONS", "MAPPING", "PROFILES", "ABOUT"};

  for (int i = 0; i < CARD_COUNT; ++i) {
    lv_obj_t *card = surface(root, 0, 0, HOME_CARD_W, HOME_CARD_H);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    s_icons[i] =
        text(card, ICONS[i], &lv_font_montserrat_14, Theme::ACCENT, 24, 11, 18);
    s_titles[i] = text(card, TITLES[i], &lv_font_montserrat_14, Theme::TEXT, 56,
                       11, HOME_CARD_W - 62);
    s_cards[i] = card;
  }

  // The column's window: four lines, with the strip that carries them behind.
  s_params = lv_obj_create(root);
  lv_obj_remove_style_all(s_params);
  lv_obj_set_pos(s_params, PARAM_X, PARAM_Y);
  lv_obj_set_size(s_params, PARAM_W, PARAM_H);
  lv_obj_set_style_bg_opa(s_params, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(s_params, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_params, LV_OBJ_FLAG_SCROLLABLE);

  // The strip is a menu-sized block, not a widget's: its pixels come from the
  // system heap, because LVGL's own pool is 64 kB and the screen's objects
  // live there.
  s_paramsPixels = (uint8_t *)std::malloc((size_t)PARAM_W * PARAM_H * 2 * 2);
  if (s_paramsPixels != NULL) {
    s_paramsCanvas = lv_canvas_create(s_params);
    lv_obj_remove_style_all(s_paramsCanvas);
    lv_canvas_set_buffer(s_paramsCanvas, s_paramsPixels, PARAM_W, PARAM_H * 2,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_paramsCanvas, 0, 0);
    lv_obj_remove_flag(s_paramsCanvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_paramsCanvas, LV_OBJ_FLAG_SCROLLABLE);
  }
  params_show();

  lv_obj_t *track = lv_obj_create(root);
  lv_obj_remove_style_all(track);
  lv_obj_set_pos(track, RAIL_X, RAIL_Y);
  lv_obj_set_size(track, 2, RAIL_H);
  lv_obj_set_style_radius(track, 1, 0);
  fill(track, RAIL);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

  s_thumb = lv_obj_create(track);
  lv_obj_remove_style_all(s_thumb);
  lv_obj_set_pos(s_thumb, 0, 0);
  lv_obj_set_size(s_thumb, 2, THUMB_H);
  lv_obj_set_style_radius(s_thumb, 1, 0);
  fill(s_thumb, THUMB);
}

void free(void) {
  lv_anim_delete(&s_scroll, NULL);
  if (s_paramsCanvas != NULL) {
    lv_obj_delete(s_paramsCanvas);
    s_paramsCanvas = NULL;
  }
  if (s_paramsPixels != NULL) {
    std::free(s_paramsPixels);
    s_paramsPixels = NULL;
  }
  s_params = NULL;
}

// An underdamped spring: the column and the thumb reach the next place a
// little early, then settle back into it instead of stopping dead.
static int32_t home_path(const lv_anim_t *anim) {
  const float t = (float)anim->act_time / (float)anim->duration;
  const float settle =
      1.0f -
      std::exp(-6.0f * t) * (std::cos(9.0f * t) + 0.67f * std::sin(9.0f * t));
  return anim->start_value +
         (int32_t)((float)(anim->end_value - anim->start_value) * settle);
}

static void home_anim_cb(void *var, int32_t value) {
  (void)var;
  const float t = (float)value / 1000.0f;
  s_scroll = s_anim_from + s_anim_diff * t;
  layout();
}

static void home_anim_done(lv_anim_t *anim) {
  (void)anim;
  s_scroll = (float)selected();
  layout();
}

void step(int step) {
  params_roll(step);

  lv_anim_delete(&s_scroll, NULL);
  s_anim_from = s_scroll;
  s_anim_diff = (float)step;

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, &s_scroll);
  lv_anim_set_values(&anim, 0, 1000);
  lv_anim_set_exec_cb(&anim, home_anim_cb);
  lv_anim_set_completed_cb(&anim, home_anim_done);
  lv_anim_set_duration(&anim, 480);
  lv_anim_set_path_cb(&anim, home_path);
  lv_anim_start(&anim);
}

void refresh(void) {
  // The strip prints the link too, and a value that moved is repainted where
  // it stands: only a move of the mark rolls it.
  params_show();
  layout();
}

} // namespace ThetaGP::Ui::Home
