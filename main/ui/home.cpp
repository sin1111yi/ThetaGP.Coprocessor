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

// The link line: the panel's own name while it is alone, and both names
// rolling upwards through it once the board answers.
#define LINK_SLOT_X PARAM_X
#define LINK_SLOT_Y 110
#define LINK_SLOT_W PARAM_W
#define LINK_SLOT_H 18
#define BAND_CHARS  32 // the most a line carries, whatever the link reports

static const char *const ICONS[CARD_COUNT] = {
    LV_SYMBOL_WIFI, LV_SYMBOL_DIRECTORY, LV_SYMBOL_KEYBOARD, LV_SYMBOL_SETTINGS,
    LV_SYMBOL_FILE};

static const char *const KEYS[CARD_COUNT][3] = {
    {"LINK", "RATE", "PROFILE"},  {"ID", "COUNT", "NAME"},
    {"KEYS", "LAYER", "MODE"},    {"INPUT", "REPORT", "SAVE"},
    {"BOARD", "MCU", "FIRMWARE"},
};

static float s_scroll;
static float s_anim_from;
static float s_anim_diff;
static lv_obj_t *s_cards[CARD_COUNT];
static lv_obj_t *s_titles[CARD_COUNT];
static lv_obj_t *s_icons[CARD_COUNT];
static lv_obj_t *s_keys[3];
static lv_obj_t *s_values[3];
static lv_obj_t *s_thumb;
static lv_obj_t *s_ticker;
static uint8_t s_drawn = 0xFF;

static char s_bandLine[128];
// Whether the band turns: it only does once the main board is answering.
static bool s_bandRolling = false;
static lv_obj_t *s_band;
static uint8_t *s_bandPixels;
static void band_free(void);
static void band_build(const char *first, const char *second);
static void band_timer(lv_timer_t *timer);
static int32_t band_path(const lv_anim_t *anim);
static void show_link(void);

// ── the parameters column ──

// The right column reads the model, so the menu says what the pages say.
static void params(uint8_t item, char out[3][26]) {
  const Model &m = model();
  switch (item) {
  case 0:
    snprintf(out[0], 26, "%s", m.link ? "live" : "waiting");
    snprintf(out[1], 26, "%u Hz", (unsigned)m.report_hz);
    snprintf(out[2], 26, "%s %u/%u", m.profile_name, (unsigned)m.profile_id,
             (unsigned)m.profile_count);
    break;
  case 1:
    snprintf(out[0], 26, "%u", (unsigned)m.profile_id);
    snprintf(out[1], 26, "%u", (unsigned)m.profile_count);
    snprintf(out[2], 26, "%s", m.profile_name);
    break;
  case 2:
    snprintf(out[0], 26, "16");
    snprintf(out[1], 26, "1");
    snprintf(out[2], 26, "hold");
    break;
  case 3:
    snprintf(out[0], 26, "button");
    snprintf(out[1], 26, "%u Hz", (unsigned)m.report_hz);
    snprintf(out[2], 26, "auto");
    break;
  default:
    snprintf(out[0], 26, "%s", m.board);
    snprintf(out[1], 26, "ESP32-S3");
    snprintf(out[2], 26, "%s", BuildInfo::VERSION);
    break;
  }
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

  if (chosen != s_drawn) {
    s_drawn = (uint8_t)chosen;
    char value[3][26];
    params((uint8_t)chosen, value);
    for (int row = 0; row < 3; ++row) {
      lv_label_set_text(s_keys[row], KEYS[chosen][row]);
      lv_label_set_text(s_values[row], value[row]);
    }
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

// ── the link line ──

// Keeps the first `cap` characters of a line and drops the rest.
static void cut(char *line, size_t cap) {
  if (std::strlen(line) > cap) {
    line[cap] = '\0';
  }
}

// Paints the line for the state the model carries, once per change. It rests
// on its first entry and the timer turns it: the answer is worth reading
// before it scrolls away.
static void show_link(void) {
  // The model's fields are bounded, so these hold a whole identifier and
  // version; the line itself carries the first BAND_CHARS of the pair.
  char first[48];
  char second[48] = "ThetaGP.Coprocessor";

  const Model &m = model();
  const bool identified = m.link && m.peer_id[0] != '\0';
  if (identified) {
    snprintf(first, sizeof first, "%s %s", m.peer_id, m.peer_version);
    cut(first, BAND_CHARS);
  } else {
    // Alone, the panel holds its own name still: nothing about a link that is
    // not there is worth a moving line.
    snprintf(first, sizeof first, "ThetaGP.Coprocessor");
    snprintf(second, sizeof second, "ThetaGP.Coprocessor");
  }

  char key[128];
  snprintf(key, sizeof key, "%s|%s", first, second);
  if (std::strcmp(s_bandLine, key) == 0) {
    return;
  }
  std::strncpy(s_bandLine, key, sizeof s_bandLine - 1);
  s_bandLine[sizeof s_bandLine - 1] = '\0';
  band_build(first, second);

  s_bandRolling = identified;
}

// The band is drawn once into a bitmap and slid, not drawn again every frame:
// blending glyphs costs milliseconds, copying a strip costs tens of
// microseconds, and a slow crawl is only smooth when every step lands on time.
#define BAND_TEXT  14 // the glyphs of Montserrat 12
#define BAND_PAD   2  // air above and below every line
#define BAND_LIFT  2  // the glyphs sit low in their own box
#define BAND_LINE  (BAND_TEXT + BAND_PAD * 2)
#define BAND_LINES 3 // the pair, and the first one again
#define BAND_H     (BAND_LINE * BAND_LINES)
#define BAND_MS    900  // one line's turn, damped
#define BAND_REST  2100 // how long a line is given once it is in place
#define BAND_TURN  (BAND_MS + BAND_REST)

static int s_bandY; // 0 or -BAND_LINE: which of the two lines is in the window

static void band_y_cb(void *var, int32_t value) {
  (void)var;
  if (s_band != NULL) {
    lv_obj_set_y(s_band, (int)value);
  }
}

// One line leaves the window and the next takes its place; after two of those
// the content is where it started, so the bitmap rewinds under it unseen.
static void band_step_done(lv_anim_t *anim) {
  (void)anim;
  s_bandY = s_bandY - BAND_LINE;
  if (s_bandY <= -(BAND_LINE * 2)) {
    s_bandY = 0;
  }
  if (s_band != NULL) {
    lv_obj_set_y(s_band, s_bandY);
  }
}

static void band_timer(lv_timer_t *timer) {
  (void)timer;
  if (s_band == NULL || !s_bandRolling) {
    return;
  }

  lv_anim_delete(s_band, band_y_cb);
  lv_anim_t step;
  lv_anim_init(&step);
  lv_anim_set_var(&step, s_band);
  lv_anim_set_values(&step, s_bandY, s_bandY - BAND_LINE);
  lv_anim_set_exec_cb(&step, band_y_cb);
  lv_anim_set_path_cb(&step, band_path);
  lv_anim_set_duration(&step, BAND_MS);
  lv_anim_set_completed_cb(&step, band_step_done);
  lv_anim_start(&step);
}

// A damped turn: the lines start slowly, run through the middle and are
// settled well before the turn is over, so the rest at the end is a rest and
// not a crawl.
static int32_t band_path(const lv_anim_t *anim) {
  const float t = (float)anim->act_time / (float)anim->duration;
  const float settle =
      1.0f -
      std::exp(-9.0f * t) * (std::cos(11.0f * t) + 0.82f * std::sin(11.0f * t));
  return anim->start_value +
         (int32_t)((float)(anim->end_value - anim->start_value) * settle);
}

// Paints the pair twice into the bitmap, so the lines leaving the top are the
// ones arriving at the bottom.
static void band_paint(const char *first, const char *second, int width) {
  if (s_band == NULL) {
    return;
  }

  lv_canvas_fill_bg(s_band, colour(Theme::BG), LV_OPA_COVER);

  lv_layer_t layer;
  lv_canvas_init_layer(s_band, &layer);

  // The entries alternate, and the band holds three of them so the copy that
  // leaves the top is the one arriving at the bottom.
  const char *entries[BAND_LINES] = {first, second, first};
  for (int i = 0; i < BAND_LINES; ++i) {
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.font = &lv_font_montserrat_12;
    dsc.color = colour(Theme::MUTED);
    dsc.text = entries[i];
    dsc.text_length = (uint32_t)std::strlen(entries[i]);
    // The line ends on the column's right mark, so it reads as the last of the
    // parameters rather than as a caption of its own.
    dsc.align = LV_TEXT_ALIGN_RIGHT;

    // Two pixels of air above the glyphs, two below: the band is drawn line by
    // line so that padding is exact rather than a guess at line spacing.
    const lv_coord_t top = (lv_coord_t)(i * BAND_LINE + BAND_PAD - BAND_LIFT);
    lv_area_t area = {0, top, (lv_coord_t)(width - 1),
                      (lv_coord_t)(top + BAND_TEXT - 1)};
    lv_draw_label(&layer, &dsc, &area);
  }

  lv_canvas_finish_layer(s_band, &layer);
}

// Lets go of the bitmap and the canvas that showed it.
static void band_free(void) {
  if (s_band != NULL) {
    lv_obj_delete(s_band);
    s_band = NULL;
  }
  if (s_bandPixels != NULL) {
    std::free(s_bandPixels);
    s_bandPixels = NULL;
  }
}

static void band_build(const char *first, const char *second) {
  const int width = LINK_SLOT_W;

  band_free();

  // The bitmap is a menu-sized block, not a widget's: it comes from the system
  // heap, because LVGL's own pool is 64 kB and the panel's objects live there.
  s_bandPixels = (uint8_t *)std::malloc((size_t)width * BAND_H * 2);
  if (s_bandPixels == NULL) {
    return;
  }

  s_band = lv_canvas_create(s_ticker);
  // The default style pads and frames its content, which would push the bitmap
  // down inside the line and clip it at the bottom.
  lv_obj_remove_style_all(s_band);
  lv_canvas_set_buffer(s_band, s_bandPixels, width, BAND_H,
                       LV_COLOR_FORMAT_RGB565);
  lv_obj_set_pos(s_band, 0, 0);
  lv_obj_remove_flag(s_band, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_band, LV_OBJ_FLAG_SCROLLABLE);

  band_paint(first, second, width);

  s_bandY = 0;
  lv_obj_set_y(s_band, 0);
}

// ── the page ──

void build(lv_obj_t *root) {
  s_scroll = (float)selected();

  // The cards read as labels rather than as sentences: upper case, and the
  // words standing clear of the icon.
  static const char *const TITLES[CARD_COUNT] = {"STATUS", "PROFILES", "KEYMAP",
                                                 "CONFIG", "ABOUT"};

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

  lv_obj_t *block = lv_obj_create(root);
  lv_obj_remove_style_all(block);
  lv_obj_set_pos(block, PARAM_X, 0);
  lv_obj_set_size(block, PARAM_W, 140);
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(block, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE);
  for (int row = 0; row < 3; ++row) {
    lv_obj_t *item = lv_obj_create(block);
    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, 0, 31 + row * 22);
    lv_obj_set_size(item, PARAM_W, 18);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(item, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(item, 10, 0);

    s_keys[row] = lv_label_create(item);
    lv_obj_set_style_text_font(s_keys[row], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_keys[row], colour(Theme::MUTED), 0);
    lv_obj_set_style_text_letter_space(s_keys[row], 1, 0);

    s_values[row] = lv_label_create(item);
    lv_obj_set_style_text_font(s_values[row], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_values[row], colour(Theme::TEXT), 0);
  }
  for (int row = 0; row < 3; ++row) {
    lv_obj_t *rule = lv_obj_create(block);
    lv_obj_remove_style_all(rule);
    lv_obj_set_pos(rule, 0, 28 + row * 22);
    lv_obj_set_size(rule, PARAM_W, 1);
    lv_obj_set_style_bg_color(rule, colour(Theme::BORDER), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_remove_flag(rule, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(rule, LV_OBJ_FLAG_SCROLLABLE);
  }
  s_drawn = 0xFF;

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

  // The link line holds the band that turns through it; the canvas inside is
  // as tall as the band, and this is the window it shows one entry through.
  s_ticker = lv_obj_create(root);
  lv_obj_remove_style_all(s_ticker);
  lv_obj_set_pos(s_ticker, LINK_SLOT_X, LINK_SLOT_Y);
  lv_obj_set_size(s_ticker, LINK_SLOT_W, LINK_SLOT_H);
  lv_obj_remove_flag(s_ticker, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_ticker, LV_OBJ_FLAG_SCROLLABLE);

  show_link();
}

void free(void) {
  lv_anim_delete(&s_scroll, NULL);
  band_free();
  s_ticker = NULL;
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
  show_link();
  // The parameters print the link too, and they only refill when the chosen
  // card moves.
  s_drawn = 0xFF;
  layout();
}

void showLink(void) { show_link(); }

void begin(void) { lv_timer_create(band_timer, BAND_TURN, NULL); }

} // namespace ThetaGP::Ui::Home
