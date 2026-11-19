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

#include "ui/model.h"
#include "ui/pages/home.h"
#include "ui/pages/pages.h"
#include "ui/profile/profile.h"
#include "ui/splash.h"
#include "ui/theme.h"

namespace ThetaGP::Ui {

namespace Menu {

using Pages::Item;
using Pages::Page;
using Theme::colour;
using Theme::text;

static lv_obj_t *s_root;

// Whether the profiles have the screen: the home menu's PROFILES card is the
// way in, and one back out of the level returns to it.
static bool s_inProfile;
static const Page *s_page;
static const Page *s_stack[6];
static uint8_t s_depth;
static uint8_t s_selected;

// ── pages ──

static const Page PAGE_MONITOR = {"Monitor", Pages::drawMonitor, NULL, 0};
static const Page PAGE_OPTIONS = {"Options", Pages::drawOptions, NULL, 4};
static const Page PAGE_MAPPING = {"Mapping", Pages::drawMapping, NULL, 16};
static const Page PAGE_PROFILES = {"Profiles", Pages::drawProfiles, NULL, 3};
static const Page PAGE_ABOUT = {"About", Pages::drawAbout, NULL, 0};

static const Item MAIN_ITEMS[] = {
    {"Monitor", &PAGE_MONITOR}, {"Options", &PAGE_OPTIONS},
    {"Mapping", &PAGE_MAPPING}, {"Profiles", &PAGE_PROFILES},
    {"About", &PAGE_ABOUT},
};

static const Page PAGE_MAIN = {"Menu", NULL, MAIN_ITEMS, 5};

// ── rendering ──

static void render(void) {
  // The page is about to be rebuilt under the wordmark: drop it early so the
  // fade callbacks never touch objects the clean is about to delete. A page
  // reached from a key press is the wordmark's end as well.
  Splash::drop();
  Home::free();
  Profile::free();
  lv_obj_clean(s_root);

  if (s_inProfile) {
    Profile::build(s_root);
    Profile::layout();
    return;
  }

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
    s_page->draw(body, s_selected);
    return;
  }

  Pages::drawItems(body, s_page->items, s_page->count, s_selected);
}

void refresh(void) {
  if (s_inProfile) {
    Profile::layout();
  } else if (s_page == &PAGE_MAIN) {
    Home::refresh();
  } else {
    render();
  }
}

// ── navigation ──

static void show(const Page *page) {
  s_page = page;
  // The main page keeps the place its column was left in; every page behind it
  // starts at its own.
  if (page == &PAGE_PROFILES) {
    s_selected =
        model().profile_id > 0 ? (uint8_t)(model().profile_id - 1) : 0;
    s_selected %= PAGE_PROFILES.count;
  } else if (page != &PAGE_MAIN) {
    s_selected = 0;
  }
  render();
}

static void move(int step) {
  if (s_inProfile) {
    Profile::move(step);
    return;
  }
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
  if (s_inProfile) {
    Profile::enter();
    return;
  }
  // The PROFILES card is the way into the level the profiles own.
  if (s_page == &PAGE_MAIN && Home::selected() == 3) {
    s_inProfile = true;
    render();
    return;
  }
  if (s_page == &PAGE_MAIN) {
    const Item *item = &MAIN_ITEMS[Home::selected()];
    if (item->page != NULL && s_depth < 6) {
      s_stack[s_depth++] = s_page;
      show(item->page);
    }
    return;
  }

  if (s_page == &PAGE_PROFILES) {
    setProfile((uint8_t)(s_selected + 1), 3, Pages::profileName(s_selected));
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
  if (s_inProfile) {
    // One step back inside the level is the level's own; no step left is the
    // way out of it.
    if (!Profile::leave()) {
      s_inProfile = false;
      render();
    }
    return;
  }
  if (s_depth == 0) {
    return;
  }
  const Page *page = s_stack[--s_depth];
  show(page);
}

Screen screen() {
  if (s_inProfile) {
    return {PageId::Profiles, Profile::item()};
  }
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

  show(&PAGE_MAIN);
  Splash::show(s_root);
}

bool introDone() { return Splash::done(); }

} // namespace Menu

} // namespace ThetaGP::Ui
