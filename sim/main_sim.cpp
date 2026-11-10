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
// into a shared frame buffer that stream.py serves as MJPEG. It stands in for
// the panel, not for the board: the flush writes memory instead of talking to
// SPI.

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "conf/board.h"
#include "lvgl.h"
#include "ui/menu.h"

// The panel is 156x428 native and mounted on its side, so the surface the menu
// draws on is the quarter turn: 428 wide by 156 tall.
#define SIM_W LCD_V_RES
#define SIM_H LCD_H_RES

#define FRAME_MAGIC 0x54475053u

typedef struct {
  uint32_t magic;
  volatile uint32_t seq;
  uint32_t width;
  uint32_t height;
} frame_header_t;

static uint8_t *s_frame;
static frame_header_t *s_header;
static uint8_t *s_draw;

static uint32_t tickMs() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void publish() {
  // The reader samples only between an even sequence number and its next
  // change, so a half-written frame is never shown.
  __sync_synchronize();
  ++s_header->seq;
  __sync_synchronize();
  memcpy(s_frame, s_draw, (size_t)SIM_W * SIM_H * 2);
  __sync_synchronize();
  ++s_header->seq;
  __sync_synchronize();
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  (void)area;
  (void)px;
  publish();
  lv_display_flush_ready(disp);
}

static void createDisplay() {
  lv_display_t *disp = lv_display_create(SIM_W, SIM_H);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, s_draw, NULL, SIM_W * SIM_H * 2,
                         LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_default(disp);
}

static void readKeys() {
  char keys[32];
  const ssize_t got = read(STDIN_FILENO, keys, sizeof(keys));
  for (ssize_t i = 0; i < got; ++i) {
    switch (keys[i]) {
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
}

// Two rectangles and a column at known places, to tell a broken pipeline from a
// broken layout.
static void testPattern() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  struct {
    int x, y, w, h;
    lv_color_t colour;
  } boxes[] = {
      {0, 0, 20, 10, lv_color_make(255, 0, 0)},
      {SIM_W - 20, SIM_H - 10, 20, 10, lv_color_make(0, 255, 0)},
      {100, 0, 4, SIM_H, lv_color_make(255, 255, 255)},
  };

  for (const auto &box : boxes) {
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

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "/tmp/opencode/thetagp_frame.bin";
  const size_t pixels = (size_t)SIM_W * SIM_H * 2;

  const int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    perror("open frame file");
    return 1;
  }
  if (ftruncate(fd, (off_t)(sizeof(frame_header_t) + pixels)) != 0) {
    perror("size frame file");
    return 1;
  }
  void *map = mmap(NULL, sizeof(frame_header_t) + pixels,
                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    perror("map frame file");
    return 1;
  }
  s_header = (frame_header_t *)map;
  s_frame = (uint8_t *)map + sizeof(frame_header_t);
  s_header->magic = FRAME_MAGIC;
  s_header->width = SIM_W;
  s_header->height = SIM_H;

  s_draw = (uint8_t *)calloc(1, pixels);
  if (s_draw == NULL) {
    fprintf(stderr, "no draw buffer\n");
    return 1;
  }

  lv_init();
  lv_tick_set_cb(tickMs);
  createDisplay();

  if (argc > 2 && strcmp(argv[2], "test") == 0) {
    testPattern();
    printf("sim: test pattern\n");
    for (;;) {
      sleep(1);
    }
  }

  ThetaGP::Ui::Menu::init();
  ThetaGP::Ui::Menu::setLink(false);
  ThetaGP::Ui::Menu::setProfile(1, 2, "Main");
  ThetaGP::Ui::Menu::setReportHz(8000);
  lv_refr_now(NULL);
  publish();
  printf("sim: %dx%d, keys on stdin: n=next e=enter b=back\n", SIM_W, SIM_H);
  fflush(stdout);

  // The stream is the only consumer of stdin; it forwards navigation keys.
  const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  for (;;) {
    uint32_t idle = lv_timer_handler();
    readKeys();
    if (idle < 2) {
      idle = 2;
    }
    if (idle > 20) {
      idle = 20;
    }
    struct timespec pause = {.tv_sec = 0, .tv_nsec = (long)idle * 1000000L};
    nanosleep(&pause, NULL);
  }
}
