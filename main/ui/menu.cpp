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

#include "build_info.h"
#include "conf/board.h"
#include "ui/theme.h"

#include "driver/gpio.h"

namespace ThetaGP::Ui {

// The device the panel speaks for. The protocol is not wired in yet, so the
// firmware fills it with the values the display was built against.
static Model s_model;

const Model &model() {
  return s_model;
}

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

// The main menu is a column of cards that scrolls under the top edge, with an
// arc in the right third showing how far down the column the mark sits.
#define CARD_COUNT 5
#define CARD_W     258
#define CARD_H     62
#define CARD_GAP   8
#define CARD_PITCH (CARD_H + CARD_GAP)

#define LIST_W 272
#define LIST_H 140

#define ARC_SIZE 132
#define ARC_X   284
#define ARC_Y   4

static lv_obj_t *s_list;
static lv_obj_t *s_cards[CARD_COUNT];
static lv_obj_t *s_card_titles[CARD_COUNT];
static lv_obj_t *s_card_numbers[CARD_COUNT];
static lv_obj_t *s_arc;
static lv_obj_t *s_arc_title;
static lv_obj_t *s_arc_index;
static uint8_t s_drawn = 0xFF;

// ── pages ──

static void draw_status(lv_obj_t *parent) {
  lv_obj_t *profile = surface(parent, 8, 8, 198, 70);
  text(profile, "PROFILE", &lv_font_montserrat_12, Theme::MUTED, 10, 6, 90);
  text(profile, s_model.profile_name, &lv_font_montserrat_20, Theme::TEXT, 10, 22, 170);

  char body[32];
  snprintf(body, sizeof body, "profile %u of %u", (unsigned)s_model.profile_id,
           (unsigned)s_model.profile_count);
  text(profile, body, &lv_font_montserrat_12, Theme::MUTED, 10, 48, 170);

  lv_obj_t *link = surface(parent, 214, 8, 198, 70);
  text(link, "LINK", &lv_font_montserrat_12, Theme::MUTED, 10, 6, 90);

  lv_obj_t *dot = lv_obj_create(link);
  lv_obj_remove_style_all(dot);
  lv_obj_set_pos(dot, 10, 26);
  lv_obj_set_size(dot, 8, 8);
  lv_obj_set_style_radius(dot, 4, 0);
  lv_obj_set_style_bg_color(dot, colour(s_model.link ? Theme::ACCENT : Theme::MUTED), 0);
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
  static const char *keys[16] = {"B1", "B2", "B3", "B4", "L1", "R1", "L2", "R2",
                                 "S1", "S2", "L3", "R3", "A1", "A2", "UP", "DN"};
  const int cell_w = 30;
  const int cell_h = 28;
  const int gap = 3;

  for (int i = 0; i < 16; ++i) {
    const int col = i % 8;
    const int row = i / 8;
    lv_obj_t *key = surface(parent, 6 + col * (cell_w + gap),
                            10 + row * (cell_h + gap), cell_w, cell_h);
    lv_obj_set_style_radius(key, 5, 0);
    if (i == 0) {
      fill(key, Theme::SURFACE_HI);
      lv_obj_set_style_border_color(key, colour(Theme::ACCENT), 0);
    }

    lv_obj_t *name = lv_label_create(key);
    lv_label_set_text(name, keys[i]);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(name, colour(i == 0 ? Theme::ACCENT : Theme::TEXT), 0);
    lv_obj_center(name);
  }
}

static void draw_profiles(lv_obj_t *parent) {
  static const char *names[3] = {"Main", "Racing", "Retro"};
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *row = surface(parent, 8, 8 + i * 24, 404, 21);
    lv_obj_set_style_radius(row, 5, 0);
    const bool active = (uint32_t)i == s_model.profile_id;
    if (active) {
      fill(row, Theme::SURFACE_HI);
      lv_obj_set_style_border_color(row, colour(Theme::ACCENT), 0);
    } else {
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(row, 0, 0);
    }

    text(row, names[i], &lv_font_montserrat_14,
         active ? Theme::ACCENT : Theme::TEXT, 10, 1, 300);
  }
}

static void draw_config(lv_obj_t *parent) {
  static const char *keys[4] = {"usb.input_mode", "led.bri", "map.socd",
                                "usb.wired_report_hz"};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t *row = surface(parent, 8, 6 + i * 22, 404, 19);
    lv_obj_set_style_radius(row, 5, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    text(row, keys[i], &lv_font_montserrat_12, Theme::TEXT, 10, 1, 300);
  }
}

static void draw_about(lv_obj_t *parent) {
  char version[40];
  snprintf(version, sizeof version, "firmware %s", BuildInfo::VERSION);

  text(parent, "ThetaGP coprocessor", &lv_font_montserrat_14, Theme::ACCENT, 10,
       8, 400);
  text(parent, version, &lv_font_montserrat_12, Theme::TEXT, 10, 30, 400);
  text(parent, "ESP32-S3 + NV3007", &lv_font_montserrat_12, Theme::MUTED, 10,
       50, 400);
  text(parent, "schema ThetaGP.PB", &lv_font_montserrat_12, Theme::MUTED, 10,
       68, 400);
}

static const Page PAGE_STATUS = {"Status", draw_status, NULL, 0};
static const Page PAGE_PROFILES = {"Profiles", draw_profiles, NULL, 0};
static const Page PAGE_KEYMAP = {"Keymap", draw_keymap, NULL, 0};
static const Page PAGE_CONFIG = {"Config", draw_config, NULL, 0};
static const Page PAGE_ABOUT = {"About", draw_about, NULL, 0};

static const Item MAIN_ITEMS[] = {
    {"Status", &PAGE_STATUS},   {"Profiles", &PAGE_PROFILES},
    {"Keymap", &PAGE_KEYMAP},   {"Config", &PAGE_CONFIG},
    {"About", &PAGE_ABOUT},
};

static const Page PAGE_MAIN = {"Menu", NULL, MAIN_ITEMS, 5};

// ── the main menu ──

// The scroll offset that puts the chosen card across the middle of the list.
static int list_offset(void) {
  const int content = CARD_COUNT * CARD_PITCH - CARD_GAP;
  int y = (int)s_selected * CARD_PITCH + CARD_H / 2 - LIST_H / 2;
  if (y < 0) {
    y = 0;
  }
  if (y > content - LIST_H) {
    y = content - LIST_H;
  }
  return y;
}

static void list_update(bool animate) {
  if (s_list == NULL) {
    return;
  }

  lv_obj_scroll_to_y(s_list, list_offset(), animate ? LV_ANIM_ON : LV_ANIM_OFF);

  if (s_selected == s_drawn) {
    return;
  }
  s_drawn = s_selected;

  for (int i = 0; i < CARD_COUNT; ++i) {
    if (s_cards[i] == NULL) {
      continue;
    }
    const bool on = i == s_selected;
    fill(s_cards[i], on ? Theme::SURFACE_HI : Theme::SURFACE);
    lv_obj_set_style_border_color(
        s_cards[i], colour(on ? Theme::ACCENT : Theme::BORDER), 0);
    lv_obj_set_style_text_color(s_card_titles[i],
                                colour(on ? Theme::TEXT : Theme::MUTED), 0);
    lv_obj_set_style_text_color(s_card_numbers[i],
                                colour(on ? Theme::ACCENT : Theme::MUTED), 0);
  }

  if (s_arc != NULL) {
    lv_arc_set_value(s_arc,
                     8 + (int32_t)(84 * s_selected) / (CARD_COUNT - 1));
  }
  if (s_arc_title != NULL) {
    lv_label_set_text(s_arc_title, MAIN_ITEMS[s_selected].label);
  }
  if (s_arc_index != NULL) {
    char index[12];
    snprintf(index, sizeof index, "%u/%u", (unsigned)s_selected + 1, CARD_COUNT);
    lv_label_set_text(s_arc_index, index);
  }
}

static void list_build(lv_obj_t *parent) {
  static const char *titles[CARD_COUNT] = {"Status", "Profiles", "Keymap",
                                           "Config", "About"};
  static const char *subs[CARD_COUNT] = {"device", "saved", "inputs",
                                         "keys", "build"};

  s_list = lv_obj_create(parent);
  lv_obj_remove_style_all(s_list);
  lv_obj_set_pos(s_list, 0, 0);
  lv_obj_set_size(s_list, LIST_W, LIST_H);
  lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_elastic(s_list, false);
  lv_obj_set_style_anim_duration(s_list, 220, 0);

  for (int i = 0; i < CARD_COUNT; ++i) {
    lv_obj_t *card = surface(s_list, 2, i * CARD_PITCH, CARD_W, CARD_H);

    char number[8];
    snprintf(number, sizeof number, "%02d", i + 1);
    s_card_numbers[i] =
        text(card, number, &lv_font_montserrat_12, Theme::ACCENT, 10, 7, 30);
    s_card_titles[i] = text(card, titles[i], &lv_font_montserrat_14,
                            Theme::TEXT, 10, 23, CARD_W - 20);
    text(card, subs[i], &lv_font_montserrat_12, Theme::MUTED, 10, 44,
         CARD_W - 20);
    s_cards[i] = card;
  }
}

// A label centred inside the arc.
static lv_obj_t *caption(lv_obj_t *parent, const char *value,
                         const lv_font_t *font, uint32_t rgb, int y) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, value);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, colour(rgb), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(label, ARC_X, y);
  lv_obj_set_width(label, ARC_SIZE);
  return label;
}

static void arc_build(lv_obj_t *parent) {
  s_arc = lv_arc_create(parent);
  lv_obj_set_pos(s_arc, ARC_X, ARC_Y);
  lv_obj_set_size(s_arc, ARC_SIZE, ARC_SIZE);
  lv_obj_set_clickable(s_arc, false);
  lv_arc_set_rotation(s_arc, 135);
  lv_arc_set_bg_angles(s_arc, 0, 270);
  lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);

  lv_obj_set_style_bg_opa(s_arc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_arc, 0, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_arc, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_color(s_arc, colour(Theme::BORDER), LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(s_arc, false, LV_PART_MAIN);

  lv_obj_set_style_arc_width(s_arc, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(s_arc, colour(Theme::ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(s_arc, false, LV_PART_INDICATOR);

  s_arc_title = caption(parent, MAIN_ITEMS[s_selected].label,
                        &lv_font_montserrat_14, Theme::TEXT, 48);
  s_arc_index = caption(parent, "1/5", &lv_font_montserrat_12, Theme::MUTED, 70);
}

// ── rendering ──

static void render(void) {
  lv_obj_clean(s_root);
  s_list = NULL;
  s_arc = NULL;
  s_arc_title = NULL;
  s_arc_index = NULL;

  if (s_page == &PAGE_MAIN) {
    list_build(s_root);
    arc_build(s_root);
    s_drawn = 0xFF;
    list_update(false);
    return;
  }


  lv_obj_t *mark = lv_obj_create(s_root);
  lv_obj_remove_style_all(mark);
  lv_obj_set_pos(mark, 10, 9);
  lv_obj_set_size(mark, 6, 6);
  lv_obj_set_style_radius(mark, 3, 0);
  lv_obj_set_style_bg_color(mark, colour(Theme::ACCENT), 0);
  lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);

  text(s_root, "ThetaGP", &lv_font_montserrat_20, Theme::TEXT, 22, 0, 200);
  text(s_root, s_page->title, &lv_font_montserrat_12, Theme::MUTED, 260, 6, 150);

  lv_obj_t *rule = lv_obj_create(s_root);
  lv_obj_remove_style_all(rule);
  lv_obj_set_pos(rule, 8, 25);
  lv_obj_set_size(rule, 404, 1);
  lv_obj_set_style_bg_color(rule, colour(Theme::BORDER), 0);
  lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);

  lv_obj_t *body = lv_obj_create(s_root);
  lv_obj_remove_style_all(body);
  lv_obj_set_pos(body, 0, 27);
  lv_obj_set_size(body, 420, 96);

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
  s_selected = 0;
  render();
}

void next() {
  if (s_page == &PAGE_MAIN) {
    s_selected = (uint8_t)((s_selected + 1) % CARD_COUNT);
    list_update(true);
    return;
  }
  if (s_page->items == NULL || s_page->count == 0) {
    return;
  }
  s_selected = (uint8_t)((s_selected + 1) % s_page->count);
  render();
}

void enter() {
  if (s_page == &PAGE_MAIN) {
    const Item *item = &MAIN_ITEMS[s_selected];
    if (item->page != NULL && s_depth < 6) {
      s_stack[s_depth++] = s_page;
      show(item->page);
    }
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

  show(&PAGE_MAIN);
}

void setLink(bool up) {
  s_model.link = up;
}

void setReportHz(uint32_t hz) {
  s_model.report_hz = hz;
}

void setProfile(uint32_t id, uint32_t count, const char *name) {
  s_model.profile_id = id;
  s_model.profile_count = count;
  if (name != NULL) {
    s_model.profile_name = name;
  }
}

} // namespace Menu

} // namespace ThetaGP::Ui
