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

#include "ui/menu.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "build_info.h"
#include "conf/board.h"
#include "ui/logo.h"
#include "ui/theme.h"

#include "driver/gpio.h"

namespace ThetaGP::Ui {

// The device the panel speaks for. The protocol is not wired in yet, so the
// firmware fills it with the values the display was built against.
static Model s_model;

const Model &model() { return s_model; }

namespace Menu {

using Theme::colour;
using Theme::fill;
using Theme::surface;
using Theme::text;

struct Page;

struct Item {
  const char *label;
  const Page *page;
};

struct Page {
  const char *title;
  // Draws the page's body; a page without one lists its items.
  void (*draw)(lv_obj_t *parent);
  const Item *items;
  uint8_t count;
};

static lv_obj_t *s_root;
static const Page *s_page;
static const Page *s_stack[6];
static uint8_t s_depth;
static uint8_t s_selected;

// The main menu is three columns: names on the left, a progress bar in the
// middle, the chosen page's parameters on the right. The columns travel along
// an arc as the selection moves, and wrap around at either end.
#define CARD_COUNT  5
#define HOME_CARD_W 150
#define HOME_CARD_H 34
#define HOME_PITCH  36
#define HOME_CENTER 60
#define HOME_CURVE  22
#define HOME_VIS    1.35f

static float s_scroll;
static float s_anim_from;
static float s_anim_diff;
static lv_obj_t *s_home_cards[CARD_COUNT];
static lv_obj_t *s_home_titles[CARD_COUNT];
static lv_obj_t *s_home_icons[CARD_COUNT];
static lv_obj_t *s_home_keys[3];
static lv_obj_t *s_home_values[3];
static lv_obj_t *s_home_thumb;
static lv_obj_t *s_home_ticker;
static lv_obj_t *s_home_link;
static uint8_t s_home_drawn = 0xFF;

// The wordmark fades in over the menu, holds, then fades out and uncovers it.
struct Splash {
  lv_obj_t *cover;
  lv_obj_t *logo;
  bool done = false;
};

static Splash s_splash;

// ── pages ──

static void draw_status(lv_obj_t *parent) {
  lv_obj_t *profile = surface(parent, 8, 23, 198, 70);
  text(profile, "PROFILE", &lv_font_montserrat_12, Theme::MUTED, 10, 6, 90);
  text(profile, s_model.profile_name, &lv_font_montserrat_20, Theme::TEXT, 10,
       22, 170);

  char body[32];
  snprintf(body, sizeof body, "profile %u of %u", (unsigned)s_model.profile_id,
           (unsigned)s_model.profile_count);
  text(profile, body, &lv_font_montserrat_12, Theme::MUTED, 10, 48, 170);

  lv_obj_t *link = surface(parent, 214, 23, 198, 70);
  text(link, "LINK", &lv_font_montserrat_12, Theme::MUTED, 10, 6, 90);

  lv_obj_t *dot = lv_obj_create(link);
  lv_obj_remove_style_all(dot);
  lv_obj_set_pos(dot, 10, 26);
  lv_obj_set_size(dot, 8, 8);
  lv_obj_set_style_radius(dot, 4, 0);
  lv_obj_set_style_bg_color(
      dot, colour(s_model.link ? Theme::ACCENT : Theme::MUTED), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

  text(link, s_model.link ? "live" : "waiting", &lv_font_montserrat_14,
       s_model.link ? Theme::TEXT : Theme::MUTED, 24, 21, 80);

  char rate[24];
  if (s_model.report_hz % 1000 == 0) {
    snprintf(rate, sizeof rate, "%u kHz", (unsigned)(s_model.report_hz / 1000));
  } else {
    snprintf(rate, sizeof rate, "%u Hz", (unsigned)s_model.report_hz);
  }
  text(link, rate, &lv_font_montserrat_14, Theme::ACCENT, 10, 46, 170);
}

static void draw_keymap(lv_obj_t *parent) {
  static const char *keys[16] = {"B1", "B2", "B3", "B4", "L1", "R1",
                                 "L2", "R2", "S1", "S2", "L3", "R3",
                                 "A1", "A2", "UP", "DN"};
  const int cell_w = 30;
  const int cell_h = 28;
  const int gap = 3;

  for (int i = 0; i < 16; ++i) {
    const int col = i % 8;
    const int row = i / 8;
    lv_obj_t *key = surface(parent, 6 + col * (cell_w + gap),
                            28 + row * (cell_h + gap), cell_w, cell_h);
    lv_obj_set_style_radius(key, 5, 0);
    const bool on_cursor = i == s_selected;
    if (on_cursor) {
      fill(key, Theme::SURFACE_HI);
      lv_obj_set_style_border_width(key, 0, 0);
    }

    lv_obj_t *name = lv_label_create(key);
    lv_label_set_text(name, keys[i]);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(
        name, colour(on_cursor ? Theme::ACCENT : Theme::TEXT), 0);
    lv_obj_center(name);
  }
}

static const char *const PROFILE_NAMES[3] = {"Main", "Racing", "Retro"};

static void draw_profiles(lv_obj_t *parent) {
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *row = surface(parent, 8, 23 + i * 24, 404, 21);
    lv_obj_set_style_radius(row, 5, 0);
    if ((uint8_t)i == s_selected) {
      fill(row, Theme::SURFACE_HI);
      lv_obj_set_style_border_width(row, 0, 0);
    } else {
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(row, 0, 0);
    }

    const bool active = (uint32_t)(i + 1) == s_model.profile_id;
    text(row, PROFILE_NAMES[i], &lv_font_montserrat_14,
         active ? Theme::ACCENT : Theme::TEXT, 10, 1, 300);
  }
}

static void draw_config(lv_obj_t *parent) {
  static const char *keys[4] = {"usb.input_mode", "led.bri", "map.socd",
                                "usb.wired_report_hz"};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *row = surface(parent, 8, 15 + i * 22, 404, 19);
    lv_obj_set_style_radius(row, 5, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    if ((uint8_t)i == s_selected) {
      fill(row, Theme::SURFACE_HI);
    } else {
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    }
    text(row, keys[i], &lv_font_montserrat_12,
         (uint8_t)i == s_selected ? Theme::TEXT : Theme::MUTED, 10, 1, 300);
  }
}

static void draw_about(lv_obj_t *parent) {
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

static const Page PAGE_STATUS = {"Status", draw_status, NULL, 0};
static const Page PAGE_PROFILES = {"Profiles", draw_profiles, NULL, 3};
static const Page PAGE_KEYMAP = {"Keymap", draw_keymap, NULL, 16};
static const Page PAGE_CONFIG = {"Config", draw_config, NULL, 4};
static const Page PAGE_ABOUT = {"About", draw_about, NULL, 0};

static const Item MAIN_ITEMS[] = {
    {"Status", &PAGE_STATUS}, {"Profiles", &PAGE_PROFILES},
    {"Keymap", &PAGE_KEYMAP}, {"Config", &PAGE_CONFIG},
    {"About", &PAGE_ABOUT},
};

static const Page PAGE_MAIN = {"Menu", NULL, MAIN_ITEMS, 5};

// ── the main menu ──

// The bar's amber thumb and the dark rail it runs on.
#define THUMB 0xFDBA74
#define RAIL  0x3A2A1F

static const char *const HOME_ICONS[CARD_COUNT] = {
    LV_SYMBOL_WIFI, LV_SYMBOL_DIRECTORY, LV_SYMBOL_KEYBOARD, LV_SYMBOL_SETTINGS,
    LV_SYMBOL_FILE};

static const char *const HOME_KEYS[CARD_COUNT][3] = {
    {"LINK", "RATE", "PROFILE"},  {"ID", "COUNT", "NAME"},
    {"KEYS", "LAYER", "MODE"},    {"INPUT", "REPORT", "SAVE"},
    {"BOARD", "MCU", "FIRMWARE"},
};

// The right column reads the model, so the menu says what the pages say.
static void home_params(uint8_t item, char out[3][26]) {
  switch (item) {
  case 0:
    snprintf(out[0], 26, "%s", s_model.link ? "live" : "waiting");
    snprintf(out[1], 26, "%u Hz", (unsigned)s_model.report_hz);
    snprintf(out[2], 26, "%s %u/%u", s_model.profile_name,
             (unsigned)s_model.profile_id, (unsigned)s_model.profile_count);
    break;
  case 1:
    snprintf(out[0], 26, "%u", (unsigned)s_model.profile_id);
    snprintf(out[1], 26, "%u", (unsigned)s_model.profile_count);
    snprintf(out[2], 26, "%s", s_model.profile_name);
    break;
  case 2:
    snprintf(out[0], 26, "16");
    snprintf(out[1], 26, "1");
    snprintf(out[2], 26, "hold");
    break;
  case 3:
    snprintf(out[0], 26, "button");
    snprintf(out[1], 26, "%u Hz", (unsigned)s_model.report_hz);
    snprintf(out[2], 26, "auto");
    break;
  default:
    snprintf(out[0], 26, "%s", s_model.board);
    snprintf(out[1], 26, "ESP32-S3");
    snprintf(out[2], 26, "%s", BuildInfo::VERSION);
    break;
  }
}

// The shortest distance from item i to the mark, so a step over either end
// travels one place rather than the whole column.
static float home_rel(int i) {
  float rel = (float)i - s_scroll;
  while (rel > CARD_COUNT / 2.0f) {
    rel -= CARD_COUNT;
  }
  while (rel < -CARD_COUNT / 2.0f) {
    rel += CARD_COUNT;
  }
  return rel;
}

static void home_layout(void) {
  int selected = (int)std::floor(s_scroll + 0.5f) % CARD_COUNT;
  if (selected < 0) {
    selected += CARD_COUNT;
  }

  for (int i = 0; i < CARD_COUNT; ++i) {
    const float rel = home_rel(i);
    const float away = rel < 0 ? -rel : rel;
    lv_obj_t *card = s_home_cards[i];

    if (away > HOME_VIS) {
      lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);

    const int bend = (int)(HOME_CURVE * rel * rel);
    lv_obj_set_pos(card, 6 - bend,
                   HOME_CENTER - HOME_CARD_H / 2 + (int)(rel * HOME_PITCH));
    lv_obj_set_style_opa(card, (lv_opa_t)(255 - (int)(95 * away)), 0);

    const bool on = i == selected;
    fill(card, on ? Theme::SURFACE_HI : Theme::SURFACE);
    lv_obj_set_style_border_width(card, on ? 0 : 1, 0);
    lv_obj_set_style_text_color(s_home_titles[i],
                                colour(on ? Theme::TEXT : Theme::MUTED), 0);
    lv_obj_set_style_text_color(s_home_icons[i],
                                colour(on ? Theme::ACCENT : Theme::MUTED), 0);
  }

  if (selected != s_home_drawn) {
    s_home_drawn = (uint8_t)selected;
    char value[3][26];
    home_params((uint8_t)selected, value);
    for (int row = 0; row < 3; ++row) {
      lv_label_set_text(s_home_keys[row], HOME_KEYS[selected][row]);
      lv_label_set_text(s_home_values[row], value[row]);
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
  lv_obj_set_y(s_home_thumb, (int)(t * 76));
}

// ── the link read-out ──

// ── the bottom line ──
// One line at the foot of the panel: the panel's own name while it is alone,
// and both names rolling upwards through it once the board answers.
#define LINK_SLOT_X 0
#define LINK_SLOT_Y 121
#define LINK_SLOT_W 420
#define LINK_SLOT_H 16

static char s_bandLine[128];
static lv_obj_t *s_home_band;
static uint8_t *s_bandPixels;
static void band_timer(lv_timer_t *timer);
static int32_t band_path(const lv_anim_t *anim);
static void band_free(void);
static void band_build(const char *line);
static void band_gone_cb(lv_anim_t *anim);
static void fade(lv_obj_t *obj, lv_opa_t from, lv_opa_t to,
                 lv_anim_completed_cb_t done);
static void hide_cb(lv_anim_t *anim);

// Paints the corner for the state the model carries, once per change.
static void home_link_show(void) {
  if (s_home_link == NULL) {
    return;
  }

  // The band is the only thing that says whether the main board is there: the
  // board's own name and version scroll while it answers, and the panel's name
  // sits still while it does not.
  const bool identified = s_model.link && s_model.peer_id[0] != '\0';

  if (!identified) {
    // Alone, the panel says its own name and nothing moves. The band is let go
    // only once it has faded, so the change is a dissolve and not a cut.
    if (s_home_band != NULL) {
      fade(s_home_band, LV_OPA_COVER, LV_OPA_TRANSP, band_gone_cb);
    }
    s_bandLine[0] = '\0';
    if (s_home_link != NULL) {
      lv_label_set_text(s_home_link, "ThetaGP.Coprocessor");
      fade(s_home_link, LV_OPA_TRANSP, LV_OPA_COVER, NULL);
    }
    return;
  }

  char line[128];
  snprintf(line, sizeof line, "%s %s\nThetaGP.Coprocessor\n",
           s_model.peer_id, s_model.peer_version);
  if (std::strcmp(s_bandLine, line) != 0) {
    std::strncpy(s_bandLine, line, sizeof s_bandLine - 1);
    s_bandLine[sizeof s_bandLine - 1] = '\0';
    band_build(line);
    fade(s_home_band, LV_OPA_TRANSP, LV_OPA_COVER, NULL);
    fade(s_home_link, LV_OPA_COVER, LV_OPA_TRANSP, hide_cb);
  }
}

// The band is drawn once into a bitmap and slid, not drawn again every frame:
// blending glyphs costs milliseconds, copying a strip costs tens of
// microseconds, and a slow crawl is only smooth when every step lands on time.
#define BAND_LINE 14  // one line of Montserrat 12
#define BAND_H    56  // four lines: the pair twice over
#define BAND_MS   700   // one line's turn, damped
#define BAND_REST 1300  // how long a line is given once it is in place
#define BAND_TURN (BAND_MS + BAND_REST)

static int s_bandY;  // 0 or -BAND_LINE: which of the two lines is in the window

static void band_y_cb(void *var, int32_t value) {
  (void)var;
  if (s_home_band != NULL) {
    lv_obj_set_y(s_home_band, (int)value);
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
  if (s_home_band != NULL) {
    lv_obj_set_y(s_home_band, s_bandY);
  }
}

static void band_timer(lv_timer_t *timer) {
  (void)timer;
  if (s_home_band == NULL) {
    return;
  }

  lv_anim_delete(s_home_band, band_y_cb);
  lv_anim_t step;
  lv_anim_init(&step);
  lv_anim_set_var(&step, s_home_band);
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
      1.0f - std::exp(-9.0f * t) *
                 (std::cos(11.0f * t) + 0.82f * std::sin(11.0f * t));
  return anim->start_value +
         (int32_t)((float)(anim->end_value - anim->start_value) * settle);
}

// Paints the pair twice into the bitmap, so the lines leaving the top are the
// ones arriving at the bottom.
static void band_paint(const char *line, int width) {
  if (s_home_band == NULL) {
    return;
  }

  lv_canvas_fill_bg(s_home_band, colour(Theme::BG), LV_OPA_COVER);

  lv_layer_t layer;
  lv_canvas_init_layer(s_home_band, &layer);

  lv_draw_label_dsc_t dsc;
  lv_draw_label_dsc_init(&dsc);
  dsc.font = &lv_font_montserrat_12;
  dsc.color = colour(Theme::MUTED);
  dsc.text = line;
  dsc.text_length = (uint32_t)std::strlen(line);
  dsc.align = LV_TEXT_ALIGN_CENTER;

  lv_area_t area = {0, 0, (lv_coord_t)(width - 1), BAND_H - 1};
  lv_draw_label(&layer, &dsc, &area);

  lv_canvas_finish_layer(s_home_band, &layer);
}

static void opa_cb(void *var, int32_t value) {
  lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static void hide_cb(lv_anim_t *anim) {
  if (anim->var != NULL) {
    lv_obj_add_flag((lv_obj_t *)anim->var, LV_OBJ_FLAG_HIDDEN);
  }
}

// Fades one line in over the other, so the corner changes state rather than
// replacing itself.
static void fade(lv_obj_t *obj, lv_opa_t from, lv_opa_t to,
                 lv_anim_completed_cb_t done) {
  if (obj == NULL) {
    return;
  }
  if (to != LV_OPA_TRANSP) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
  lv_anim_delete(obj, opa_cb);

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, obj);
  lv_anim_set_values(&anim, from, to);
  lv_anim_set_exec_cb(&anim, opa_cb);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
  lv_anim_set_duration(&anim, 600);
  // A fade-in ends with the line on show; only a fade-out is hidden at the end.
  lv_anim_set_completed_cb(
      &anim, done != NULL ? done : (to == LV_OPA_TRANSP ? hide_cb : nullptr));
  lv_anim_start(&anim);
}

static void band_free(void) {
  if (s_home_band != NULL) {
    lv_obj_delete(s_home_band);
    s_home_band = NULL;
  }
  if (s_bandPixels != NULL) {
    lv_free(s_bandPixels);
    s_bandPixels = NULL;
  }
}

// Builds the band for one line and starts it moving.
static void band_gone_cb(lv_anim_t *anim) {
  (void)anim;
  band_free();
}

static void band_build(const char *line) {
  const int width = LINK_SLOT_W;

  band_free();

  s_bandPixels = (uint8_t *)lv_malloc((size_t)width * BAND_H * 2);
  if (s_bandPixels == NULL) {
    return;
  }

  s_home_band = lv_canvas_create(s_home_ticker);
  lv_obj_set_style_opa(s_home_band, LV_OPA_TRANSP, 0);
  lv_canvas_set_buffer(s_home_band, s_bandPixels, width, BAND_H,
                       LV_COLOR_FORMAT_RGB565);
  lv_obj_set_pos(s_home_band, 0, 0);
  lv_obj_remove_flag(s_home_band, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_home_band, LV_OBJ_FLAG_SCROLLABLE);

  char twice[256];
  snprintf(twice, sizeof twice, "%s%s", line, line);
  band_paint(twice, width);

  s_bandY = 0;
  lv_obj_set_y(s_home_band, 0);
}

static void home_build(void) {
  s_scroll = (float)s_selected;

  // The cards read as labels rather than as sentences: upper case, and the
  // words standing clear of the icon.
  static const char *const TITLES[CARD_COUNT] = {"STATUS", "PROFILES", "KEYMAP",
                                                 "CONFIG", "ABOUT"};

  for (int i = 0; i < CARD_COUNT; ++i) {
    lv_obj_t *card = surface(s_root, 0, 0, HOME_CARD_W, HOME_CARD_H);
    lv_obj_set_style_radius(card, 7, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    s_home_icons[i] = text(card, HOME_ICONS[i], &lv_font_montserrat_14,
                           Theme::ACCENT, 24, 9, 18);
    s_home_titles[i] = text(card, TITLES[i], &lv_font_montserrat_14,
                            Theme::TEXT, 56, 9, HOME_CARD_W - 62);
    s_home_cards[i] = card;
  }

  lv_obj_t *block = lv_obj_create(s_root);
  lv_obj_remove_style_all(block);
  lv_obj_set_pos(block, 200, 0);
  lv_obj_set_size(block, 214, 156);
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(block, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE);
  for (int row = 0; row < 3; ++row) {
    lv_obj_t *item = lv_obj_create(block);
    lv_obj_remove_style_all(item);
    lv_obj_set_pos(item, 0, 31 + row * 22);
    lv_obj_set_size(item, 214, 18);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(item, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(item, 10, 0);

    s_home_keys[row] = lv_label_create(item);
    lv_obj_set_style_text_font(s_home_keys[row], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_home_keys[row], colour(Theme::MUTED), 0);
    lv_obj_set_style_text_letter_space(s_home_keys[row], 1, 0);

    s_home_values[row] = lv_label_create(item);
    lv_obj_set_style_text_font(s_home_values[row], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_home_values[row], colour(Theme::TEXT), 0);
  }
  for (int row = 0; row < 3; ++row) {
    lv_obj_t *rule = lv_obj_create(block);
    lv_obj_remove_style_all(rule);
    lv_obj_set_pos(rule, 0, 28 + row * 22);
    lv_obj_set_size(rule, 214, 1);
    lv_obj_set_style_bg_color(rule, colour(Theme::BORDER), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_remove_flag(rule, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(rule, LV_OBJ_FLAG_SCROLLABLE);
  }
  s_home_drawn = 0xFF;

  lv_obj_t *track = lv_obj_create(s_root);
  lv_obj_remove_style_all(track);
  lv_obj_set_pos(track, 176, 12);
  lv_obj_set_size(track, 2, 96);
  lv_obj_set_style_radius(track, 1, 0);
  fill(track, RAIL);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

  s_home_thumb = lv_obj_create(track);
  lv_obj_remove_style_all(s_home_thumb);
  lv_obj_set_pos(s_home_thumb, 0, 0);
  lv_obj_set_size(s_home_thumb, 2, 20);
  lv_obj_set_style_radius(s_home_thumb, 1, 0);
  fill(s_home_thumb, THUMB);

  // One line at the bottom: what this panel is until the main board answers,
  // and what the main board is once it does.
  s_home_ticker = lv_obj_create(s_root);
  lv_obj_remove_style_all(s_home_ticker);
  lv_obj_set_pos(s_home_ticker, LINK_SLOT_X, LINK_SLOT_Y);
  lv_obj_set_size(s_home_ticker, LINK_SLOT_W, LINK_SLOT_H);
  lv_obj_remove_flag(s_home_ticker, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_home_ticker, LV_OBJ_FLAG_SCROLLABLE);

  s_home_link = lv_label_create(s_home_ticker);
  lv_label_set_text(s_home_link, "ThetaGP.Coprocessor");
  lv_obj_set_style_text_font(s_home_link, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_home_link, colour(Theme::MUTED), 0);
  lv_obj_set_width(s_home_link, LINK_SLOT_W);
  lv_obj_set_style_text_align(s_home_link, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(s_home_link, LV_LABEL_LONG_CLIP);
  lv_obj_set_pos(s_home_link, 0, (LINK_SLOT_H - BAND_LINE) / 2);

  home_link_show();
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
  home_layout();
}

static void home_anim_done(lv_anim_t *anim) {
  (void)anim;
  float pos = s_scroll;
  pos -= std::floor(pos / CARD_COUNT) * CARD_COUNT;
  int selected = (int)std::floor(pos + 0.5f) % CARD_COUNT;
  s_scroll = (float)selected;
  s_selected = (uint8_t)selected;
  home_layout();
}

// One place along the column, along the arc and back into range.
static void home_step(int step) {
  s_selected = (uint8_t)((s_selected + CARD_COUNT + step) % CARD_COUNT);

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

// ── rendering ──

static void render(void) {
  lv_anim_delete(&s_scroll, NULL);
  if (s_splash.cover != NULL) {
    // The page is about to be rebuilt under the splash: drop it early so the
    // fade callbacks never touch objects the clean is about to delete. A page
    // reached from a key press is the wordmark's end as well.
    s_splash.done = true;
    lv_anim_delete(&s_splash, NULL);
    s_splash.cover = NULL;
    s_splash.logo = NULL;
  }
  band_free();
  lv_obj_clean(s_root);
  s_home_ticker = NULL;
  s_home_link = NULL;

  if (s_page == &PAGE_MAIN) {
    home_build();
    home_layout();
    return;
  }

  lv_obj_t *body = lv_obj_create(s_root);
  lv_obj_remove_style_all(body);
  lv_obj_set_pos(body, 0, 0);
  lv_obj_set_size(body, 420, 116);

  text(s_root, "BOOT  next   hold  open", &lv_font_montserrat_12, Theme::MUTED,
       10, 124, 300);

  if (s_page->draw != NULL) {
    s_page->draw(body);
    return;
  }

  for (uint8_t i = 0; i < s_page->count; ++i) {
    const bool chosen = i == s_selected;
    lv_obj_t *row = lv_obj_create(body);
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

    text(row, s_page->items[i].label, &lv_font_montserrat_14,
         chosen ? Theme::TEXT : Theme::MUTED, 12, 0, 340);
    text(row, ">", &lv_font_montserrat_14,
         chosen ? Theme::ACCENT : Theme::MUTED, 384, 0, 16);
  }
}

// ── navigation ──

static void show(const Page *page) {
  s_page = page;
  if (page == &PAGE_MAIN) {
    s_selected = (uint8_t)((int)(s_scroll + 0.5f) % CARD_COUNT);
  } else if (page == &PAGE_PROFILES) {
    s_selected = s_model.profile_id > 0 ? (uint8_t)(s_model.profile_id - 1) : 0;
    s_selected %= PAGE_PROFILES.count;
  } else {
    s_selected = 0;
  }
  render();
}

static void move(int step) {
  if (s_page == &PAGE_MAIN) {
    home_step(step);
    return;
  }
  if (s_page->count == 0) {
    return;
  }
  s_selected = (uint8_t)((s_selected + s_page->count + step) % s_page->count);
  render();
}

void next() { move(1); }

void prev() { move(-1); }

void enter() {
  if (s_page == &PAGE_MAIN) {
    const Item *item = &MAIN_ITEMS[s_selected];
    if (item->page != NULL && s_depth < 6) {
      s_stack[s_depth++] = s_page;
      show(item->page);
    }
    return;
  }

  if (s_page == &PAGE_PROFILES) {
    setProfile((uint8_t)(s_selected + 1), 3, PROFILE_NAMES[s_selected]);
    render();
    return;
  }

  if (s_page->items != NULL && s_page->count != 0) {
    const Item *item = &s_page->items[s_selected];
    if (item->page != NULL && s_depth < 6) {
      s_stack[s_depth++] = s_page;
      show(item->page);
      return;
    }
    return;
  }
  back();
}

void back() {
  if (s_depth == 0) {
    return;
  }
  const Page *page = s_stack[--s_depth];
  show(page);
}

// ── the splash, once at power-up ──

static void splash_opa(void *var, int32_t value) {
  Splash *splash = (Splash *)var;
  const lv_opa_t opa = (lv_opa_t)value;
  lv_obj_set_style_bg_opa(splash->cover, opa, 0);
  lv_obj_set_style_image_opa(splash->logo, opa, 0);
}

static void splash_gone(lv_anim_t *anim) {
  s_splash.done = true;
  (void)anim;
  if (s_splash.cover != NULL) {
    lv_obj_delete(s_splash.cover);
  }
  s_splash.cover = NULL;
  s_splash.logo = NULL;
}

static void splash(void) {
  lv_obj_t *cover = lv_obj_create(s_root);
  lv_obj_remove_style_all(cover);
  lv_obj_set_pos(cover, 0, 0);
  lv_obj_set_size(cover, 420, 140);
  lv_obj_set_style_bg_color(cover, colour(Theme::BG), 0);
  lv_obj_remove_flag(cover, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(cover, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *logo = lv_image_create(cover);
  lv_image_set_src(logo, &thetagp_logo);
  lv_obj_set_style_image_recolor(logo, lv_color_hex(0x7EB4F2), 0);
  lv_obj_set_style_image_recolor_opa(logo, LV_OPA_COVER, 0);
  lv_obj_align(logo, LV_ALIGN_CENTER, 0, 0);

  s_splash.cover = cover;
  s_splash.logo = logo;
  splash_opa(&s_splash, LV_OPA_TRANSP);

  lv_anim_t in;
  lv_anim_init(&in);
  lv_anim_set_var(&in, &s_splash);
  lv_anim_set_values(&in, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_exec_cb(&in, splash_opa);
  lv_anim_set_path_cb(&in, lv_anim_path_ease_in_out);
  lv_anim_set_duration(&in, 400);
  lv_anim_start(&in);

  lv_anim_t out;
  lv_anim_init(&out);
  lv_anim_set_var(&out, &s_splash);
  lv_anim_set_values(&out, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_exec_cb(&out, splash_opa);
  lv_anim_set_path_cb(&out, lv_anim_path_ease_in_out);
  lv_anim_set_delay(&out, 1800);
  lv_anim_set_duration(&out, 550);
  lv_anim_set_completed_cb(&out, splash_gone);
  lv_anim_start(&out);
}

// ── the board button, until the link's own buttons arrive ──

static void button_timer(lv_timer_t *timer) {
  (void)timer;
  static bool down = false;
  static uint32_t pressed_at = 0;

  const bool held = gpio_get_level(PIN_BUTTON) == 0;
  const uint32_t now = lv_tick_get();
  if (held && !down) {
    pressed_at = now;
  } else if (!held && down) {
    if (now - pressed_at >= 500) {
      enter();
    } else {
      next();
    }
  }
  down = held;
}

void init() {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, colour(Theme::BG), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  // The panel's usable window is 420x140, four columns in from either side
  // and five rows below the centre of its 428x156 surface.
  s_root = lv_obj_create(screen);
  lv_obj_remove_style_all(s_root);
  lv_obj_set_pos(s_root, 4, 13);
  lv_obj_set_size(s_root, 420, 140);

  gpio_config_t button = {};
  button.pin_bit_mask = 1ULL << PIN_BUTTON;
  button.mode = GPIO_MODE_INPUT;
  button.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&button);
  lv_timer_create(button_timer, 20, NULL);
  lv_timer_create(band_timer, BAND_TURN, NULL);

  show(&PAGE_MAIN);
  splash();
}

bool introDone() { return s_splash.done; }

void setPeer(const char *id, const char *version) {
  if (id != NULL) {
    std::strncpy(s_model.peer_id, id, sizeof s_model.peer_id - 1);
    s_model.peer_id[sizeof s_model.peer_id - 1] = '\0';
  }
  if (version != NULL) {
    std::strncpy(s_model.peer_version, version,
                 sizeof s_model.peer_version - 1);
    s_model.peer_version[sizeof s_model.peer_version - 1] = '\0';
  }
  if (s_page == &PAGE_MAIN) {
    home_link_show();
  }
}

void setLink(bool up) {
  if (s_model.link == up) {
    return;
  }
  s_model.link = up;

  if (s_page == &PAGE_MAIN) {
    home_link_show();
    // The parameters column prints the link too, and it only refills when the
    // chosen card moves.
    s_home_drawn = 0xFF;
    home_layout();
  } else if (s_page == &PAGE_STATUS) {
    render();
  }
}

void setReportHz(uint32_t hz) { s_model.report_hz = hz; }

void setProfile(uint32_t id, uint32_t count, const char *name) {
  s_model.profile_id = id;
  s_model.profile_count = count;
  if (name != NULL) {
    s_model.profile_name = name;
  }
}

} // namespace Menu

} // namespace ThetaGP::Ui
