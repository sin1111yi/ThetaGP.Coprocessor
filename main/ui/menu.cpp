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

#include <cstdio>
#include <cstring>

#include "build_info.h"
#include "conf/board.h"
#include "ui/home.h"
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

// The wordmark fades in over the menu, holds, then fades out and uncovers it.
struct Splash {
  lv_obj_t *cover;
  lv_obj_t *logo;
  bool done = false;
};

static Splash s_splash;

// ── pages ──

static void draw_monitor(lv_obj_t *parent) {
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

static void draw_mapping(lv_obj_t *parent) {
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

static void draw_options(lv_obj_t *parent) {
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

static const Page PAGE_MONITOR = {"Monitor", draw_monitor, NULL, 0};
static const Page PAGE_OPTIONS = {"Options", draw_options, NULL, 4};
static const Page PAGE_MAPPING = {"Mapping", draw_mapping, NULL, 16};
static const Page PAGE_PROFILES = {"Profiles", draw_profiles, NULL, 3};
static const Page PAGE_ABOUT = {"About", draw_about, NULL, 0};

static const Item MAIN_ITEMS[] = {
    {"Monitor", &PAGE_MONITOR}, {"Options", &PAGE_OPTIONS},
    {"Mapping", &PAGE_MAPPING}, {"Profiles", &PAGE_PROFILES},
    {"About", &PAGE_ABOUT},
};

static const Page PAGE_MAIN = {"Menu", NULL, MAIN_ITEMS, 5};

// ── rendering ──

static void render(void) {
  if (s_splash.cover != NULL) {
    // The page is about to be rebuilt under the splash: drop it early so the
    // fade callbacks never touch objects the clean is about to delete. A page
    // reached from a key press is the wordmark's end as well.
    s_splash.done = true;
    lv_anim_delete(&s_splash, NULL);
    s_splash.cover = NULL;
    s_splash.logo = NULL;
  }
  Home::free();
  lv_obj_clean(s_root);

  if (s_page == &PAGE_MAIN) {
    Home::build(s_root);
    Home::layout();
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

// The board the link reports, which the pages name.
static void setBoard(const char *id);

static void show(const Page *page) {
  s_page = page;
  // The main page keeps the place its column was left in; every page behind it
  // starts at its own.
  if (page == &PAGE_PROFILES) {
    s_selected = s_model.profile_id > 0 ? (uint8_t)(s_model.profile_id - 1) : 0;
    s_selected %= PAGE_PROFILES.count;
  } else if (page != &PAGE_MAIN) {
    s_selected = 0;
  }
  render();
}

static void move(int step) {
  if (s_page == &PAGE_MAIN) {
    Home::step(step);
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
    const Item *item = &MAIN_ITEMS[Home::selected()];
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

Screen screen() {
  if (s_page == &PAGE_MAIN) {
    return {PageId::Menu, (uint8_t)Home::selected()};
  }
  if (s_page == &PAGE_MONITOR) {
    return {PageId::Monitor, s_selected};
  }
  if (s_page == &PAGE_OPTIONS) {
    return {PageId::Options, s_selected};
  }
  if (s_page == &PAGE_MAPPING) {
    return {PageId::Mapping, s_selected};
  }
  if (s_page == &PAGE_PROFILES) {
    return {PageId::Profiles, s_selected};
  }
  return {PageId::About, s_selected};
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
  Home::begin();

  show(&PAGE_MAIN);
  splash();
}

bool introDone() { return s_splash.done; }

void setPeer(const char *id, const char *version) {
  setBoard(id);
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
    Home::showLink();
  }
}

void setLink(bool up) {
  if (s_model.link == up) {
    return;
  }
  s_model.link = up;

  if (s_page == &PAGE_MAIN) {
    Home::refresh();
  } else if (s_page == &PAGE_MONITOR) {
    render();
  }
}

// A value the main board reports, kept where the pages read it: a page that
// prints it is rebuilt only when the value really moved.
static void refreshPages(void) {
  if (s_page == &PAGE_MAIN) {
    Home::refresh();
  } else {
    render();
  }
}

void setReportHz(uint32_t hz) {
  if (hz == s_model.report_hz) {
    return;
  }
  s_model.report_hz = hz;
  refreshPages();
}

void setProfile(uint32_t id, uint32_t count, const char *name) {
  const bool same =
      id == s_model.profile_id && count == s_model.profile_count &&
      (name == NULL || std::strcmp(name, s_model.profile_name) == 0);
  if (same) {
    return;
  }
  s_model.profile_id = id;
  s_model.profile_count = count;
  if (name != NULL) {
    s_model.profile_name = name;
  }
  refreshPages();
}

// The board the link reports is the board the pages name, so the About page
// says where the screen it is on actually is.
static char s_boardId[24] = "ThetaGP-H743";

void setBoard(const char *id) {
  if (id == NULL || id[0] == '\0') {
    return;
  }
  std::strncpy(s_boardId, id, sizeof s_boardId - 1);
  s_boardId[sizeof s_boardId - 1] = '\0';
  s_model.board = s_boardId;
}

} // namespace Menu

} // namespace ThetaGP::Ui
