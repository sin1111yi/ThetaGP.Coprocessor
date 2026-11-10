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

// A host build of the menu: the same ui/menu.cpp the firmware links, drawn
// here instead of sent over SPI. Frames leave on stdout as a header and a
// window of RGB565, and navigation keys arrive on stdin, so the host side is a
// pipe and nothing else. That is what lets it run on Windows as well.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "conf/board.h"
#include "lvgl.h"
#include "ui/menu.h"

// The panel is 156x428 native and mounted on its side, so the surface the menu
// draws on is the quarter turn: 428 wide by 156 tall.
#define SIM_W LCD_V_RES
#define SIM_H LCD_H_RES

#define FRAME_MAGIC 0x54475053u

namespace {

struct FrameHeader {
  uint32_t magic;
  uint32_t width;
  uint32_t height;
};

uint8_t *s_draw;
std::atomic<int> s_key{-1};

uint32_t tickMs() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now)
      .count();
}

void publish() {
  const FrameHeader header = {FRAME_MAGIC, SIM_W, SIM_H};
  std::fwrite(&header, sizeof header, 1, stdout);
  std::fwrite(s_draw, (size_t)SIM_W * SIM_H * 2, 1, stdout);
  std::fflush(stdout);
}

void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  (void)area;
  (void)px;
  publish();
  lv_display_flush_ready(disp);
}

void createDisplay() {
  lv_display_t *disp = lv_display_create(SIM_W, SIM_H);
  lv_display_set_flush_cb(disp, flushCb);
  // One buffer holding the whole surface: every flush is the whole picture, so
  // the reader never has to reassemble a partial one.
  lv_display_set_buffers(disp, s_draw, NULL, SIM_W * SIM_H * 2,
                         LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_default(disp);
}

// A reader thread, so the loop that draws never blocks on a keyboard that has
// nothing to say. One pending key is enough: the menu advances a step at a
// time.
void readKeys() {
  for (;;) {
    const int key = std::getchar();
    if (key == EOF) {
      return;
    }
    if (key == 'n' || key == 'e' || key == 'b') {
      s_key.store(key);
    }
  }
}

void applyKey() {
  switch (s_key.exchange(-1)) {
  case 'n':
    ThetaGP::Ui::Menu::next();
    break;
  case 'e':
    ThetaGP::Ui::Menu::enter();
    break;
  case 'b':
    ThetaGP::Ui::Menu::back();
    break;
  default:
    break;
  }
}

// A rectangle in each corner and a column at a known place, to tell a broken
// pipeline from a broken layout.
void testPattern() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  struct Box {
    int x, y, w, h;
    lv_color_t colour;
  } boxes[] = {
      {0, 0, 20, 10, lv_color_make(255, 0, 0)},
      {SIM_W - 20, SIM_H - 10, 20, 10, lv_color_make(0, 255, 0)},
      {100, 0, 4, SIM_H, lv_color_make(255, 255, 255)},
  };

  for (const Box &box : boxes) {
    lv_obj_t *obj = lv_obj_create(scr);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, box.x, box.y);
    lv_obj_set_size(obj, box.w, box.h);
    lv_obj_set_style_bg_color(obj, box.colour, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  }
  lv_refr_now(NULL);
  publish();
}

} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
  // A frame is bytes, not text: keep the C runtime from translating them.
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stdin), _O_BINARY);
#endif

  s_draw = (uint8_t *)std::calloc(1, (size_t)SIM_W * SIM_H * 2);
  if (s_draw == nullptr) {
    std::fprintf(stderr, "simulate: no draw buffer\n");
    return 1;
  }

  lv_init();
  lv_tick_set_cb(tickMs);
  createDisplay();

  if (argc > 1 && std::strcmp(argv[1], "--test") == 0) {
    testPattern();
    std::fprintf(stderr, "simulate: test pattern\n");
    for (;;) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  ThetaGP::Ui::Menu::init();
  ThetaGP::Ui::Menu::setLink(false);
  ThetaGP::Ui::Menu::setProfile(1, 2, "Main");
  ThetaGP::Ui::Menu::setReportHz(8000);
  lv_refr_now(NULL);
  publish();

  std::thread reader(readKeys);
  reader.detach();

  for (;;) {
    applyKey();
    uint32_t idle = lv_timer_handler();
    if (idle < 2) {
      idle = 2;
    }
    if (idle > 20) {
      idle = 20;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(idle));
  }
}
