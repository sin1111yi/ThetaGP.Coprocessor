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

// Drives the menu through a fixed script on a fake clock and prints a hash of
// the frame after every step, so two trees can be compared byte for byte: a
// refactor that keeps the picture prints the same numbers.
//
// The baseline is this program built against the tree it is compared with --
// `git worktree add <dir> <commit>` and the same build there. The sequence the
// refactor of 2026-11 was measured against starts at `3f0c7b2`.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "lvgl.h"
#include "ui/menu.h"

using namespace ThetaGP::Ui;

namespace {

uint32_t s_now;
uint8_t *s_buf;

uint32_t tick() { return s_now; }

void flush(lv_display_t *display, const lv_area_t *, uint8_t *) {
  lv_display_flush_ready(display);
}

uint64_t hash() {
  uint64_t h = 1469598103934665603ull;
  for (int i = 0; i < 428 * 156 * 2; ++i) {
    h ^= s_buf[i];
    h *= 1099511628211ull;
  }
  return h;
}

int s_line;

void run(const char *what, int ms) {
  for (int t = 0; t < ms; t += 10) {
    s_now += 10;
    lv_timer_handler();
    std::printf("%04d %-10s +%3d %016llx\n", s_line, what, t,
                (unsigned long long)hash());
  }
  ++s_line;
}

} // namespace

int main(int argc, char **argv) {
  const bool extra = argc > 1;

  s_buf = (uint8_t *)std::calloc(1, 428 * 156 * 2);
  lv_init();
  lv_tick_set_cb(tick);
  lv_display_t *display = lv_display_create(428, 156);
  lv_display_set_flush_cb(display, flush);
  lv_display_set_buffers(display, s_buf, NULL, 428 * 156 * 2,
                         LV_DISPLAY_RENDER_MODE_FULL);

  Menu::init();
  Menu::setLink(false);
  Menu::setProfile(1, 2, "Main");
  Menu::setReportHz(8000);
  run("splash", 3200);

#define K(f, ms)                                                               \
  do {                                                                         \
    Menu::f();                                                                 \
    run(#f, ms);                                                               \
  } while (0)

  for (int i = 0; i < 6; ++i) {
    K(next, 520); // round the home cards, wrapping
  }
  K(prev, 520);
  K(prev, 520);
  K(next, 120);
  K(next, 90);
  K(prev, 60);
  K(next, 700); // moves made during moves
  Menu::setLink(true);
  run("link", 60);
  Menu::setPeer("ThetaGP-H743B", "1.2.3");
  run("peer", 60);
  Menu::setReportHz(4000);
  run("hz", 60);
  Menu::setProfile(2, 3, "Racing");
  run("profile", 60);

  // every card of the home page, and what is behind it
  for (int card = 0; card < 5; ++card) {
    while (Menu::screen().item != card) {
      K(next, 520);
    }
    K(enter, 60);
    for (int i = 0; i < 18; ++i) {
      K(next, 30);
    }
    K(prev, 30);
    if (card == 0) {
      Menu::setLink(false);
      run("link", 30);
      Menu::setReportHz(1000);
      run("hz", 30);
      Menu::setLink(true);
      run("link", 30);
    }
    if (card == 3) {
      // the profile level: names, the menu, the window, a value
      K(back, 60);
      K(enter, 60);
      K(next, 520);
      K(next, 150);
      K(next, 520);
      K(next, 520);
      K(next, 520);
      K(prev, 520);
      K(prev, 520);
      K(enter, 60);
      for (int i = 0; i < 6; ++i) {
        K(next, 500);
      }
      for (int i = 0; i < 6; ++i) {
        K(prev, 500);
      }
      K(next, 60);
      K(next, 60);
      K(next, 60);
      K(enter, 60);
      for (int i = 0; i < 5; ++i) {
        K(next, 40);
      }
      K(enter, 40);
      K(next, 40);
      K(next, 40);
      K(next, 40);
      K(enter, 40);
      K(back, 40);
      K(prev, 40);
      K(enter, 40);
      K(prev, 40);
      K(back, 40);
      Menu::setReportHz(2000);
      run("hz", 40);
      Menu::setProfile(1, 4, "Main");
      run("profile", 40);
      if (extra) {
        Menu::setLink(false);
        run("link", 40);
        Menu::setLink(true);
        run("link", 40);
      }
      K(back, 40);
      K(back, 40);
      K(prev, 520);
      K(enter, 40);
      K(back, 40);
      K(back, 60);
    }
    K(back, 60);
    K(enter, 60);
    K(enter, 60);
    K(back, 60);
  }
  K(enter, 60);
  K(back, 60);

  const Menu::Screen screen = Menu::screen();
  std::printf("end page %d item %d intro %d\n", (int)screen.page,
              (int)screen.item, (int)Menu::introDone());
  return 0;
}
