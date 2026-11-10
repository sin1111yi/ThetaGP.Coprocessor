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
// into a window here instead of sent over SPI. LVGL renders into one buffer of
// its own; the window only blits it, so nothing about the firmware's drawing
// changes when it runs on a desk.

#include <SDL.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "conf/board.h"
#include "lvgl.h"
#include "ui/menu.h"

// The panel is 156x428 native and mounted on its side, so the surface the menu
// draws on is the quarter turn: 428 wide by 156 tall.
#define SIM_W LCD_V_RES
#define SIM_H LCD_H_RES

namespace {

uint8_t *s_draw;
SDL_Renderer *s_renderer;
SDL_Texture *s_texture;
bool s_quit;

uint32_t tickMs() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now)
      .count();
}

// LVGL draws RGB565 little endian, which is the format the texture holds.
int pixelFormat() { return SDL_PIXELFORMAT_RGB565; }

void present() {
  SDL_UpdateTexture(s_texture, nullptr, s_draw, SIM_W * 2);
  SDL_RenderClear(s_renderer);
  SDL_RenderCopy(s_renderer, s_texture, nullptr, nullptr);
  SDL_RenderPresent(s_renderer);
}

void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  (void)area;
  (void)px;
  if (s_renderer != nullptr) {
    present();
  }
  lv_display_flush_ready(disp);
}

void createDisplay() {
  lv_display_t *disp = lv_display_create(SIM_W, SIM_H);
  lv_display_set_flush_cb(disp, flushCb);
  // One buffer holding the whole surface: every flush is the whole picture, so
  // the blit never has to assemble a partial one.
  lv_display_set_buffers(disp, s_draw, NULL, SIM_W * SIM_H * 2,
                         LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_default(disp);
}

// The keys the board's one button stands in for, plus a way out.
void handleKeys() {
  SDL_Event event;
  while (SDL_PollEvent(&event) != 0) {
    if (event.type == SDL_QUIT) {
      s_quit = true;
    } else if (event.type == SDL_KEYDOWN) {
      switch (event.key.keysym.sym) {
      case SDLK_UP:
      case SDLK_LEFT:
        ThetaGP::Ui::Menu::prev();
        break;
      case SDLK_DOWN:
      case SDLK_RIGHT:
        ThetaGP::Ui::Menu::next();
        break;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
        ThetaGP::Ui::Menu::enter();
        break;
      case SDLK_BACKSPACE:
        ThetaGP::Ui::Menu::back();
        break;
      default:
        break;
      }
    }
  }
}

// Writes the surface as a PPM, for a machine with no display to render on: the
// picture can then be looked at as a file.
bool dumpPpm(const char *path) {
  FILE *file = std::fopen(path, "wb");
  if (file == nullptr) {
    std::fprintf(stderr, "simulate: cannot write %s\n", path);
    return false;
  }
  std::fprintf(file, "P6\n%d %d\n255\n", SIM_W, SIM_H);

  uint8_t *row = (uint8_t *)std::malloc((size_t)SIM_W * 3);
  for (int y = 0; y < SIM_H; ++y) {
    for (int x = 0; x < SIM_W; ++x) {
      const uint16_t pixel = ((const uint16_t *)s_draw)[y * SIM_W + x];
      row[x * 3 + 0] = (uint8_t)(((pixel >> 11) & 0x1F) << 3);
      row[x * 3 + 1] = (uint8_t)(((pixel >> 5) & 0x3F) << 2);
      row[x * 3 + 2] = (uint8_t)((pixel & 0x1F) << 3);
    }
    std::fwrite(row, (size_t)SIM_W * 3, 1, file);
  }
  std::free(row);
  std::fclose(file);
  std::printf("simulate: wrote %s\n", path);
  return true;
}

void usage(const char *argv0) {
  std::printf("usage: %s [--scale F] [--dump FILE.ppm]\n", argv0);
  std::printf("  up / left          previous card\n"
              "  down / right       next card\n"
              "  enter              enter the page\n"
              "  backspace          back\n"
              "  close the window   quit\n");
}

} // namespace

int main(int argc, char **argv) {
  float scale = 1.0f;
  const char *dump = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
      scale = std::strtof(argv[++i], nullptr);
      if (scale <= 0.0f) {
        scale = 0.5f;
      }
    } else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
      dump = argv[++i];
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  s_draw = (uint8_t *)std::calloc(1, (size_t)SIM_W * SIM_H * 2);
  if (s_draw == nullptr) {
    std::fprintf(stderr, "simulate: no draw buffer\n");
    return 1;
  }

  // A dump is a picture of one state, so it configures the panel the way the
  // firmware does and writes what LVGL drew.
  if (dump == nullptr) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::fprintf(stderr, "simulate: SDL: %s\n", SDL_GetError());
      return 1;
    }
    SDL_Window *window = SDL_CreateWindow(
        "ThetaGP.Coprocessor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)(SIM_W * scale + 0.5f), (int)(SIM_H * scale + 0.5f),
        SDL_WINDOW_SHOWN);
    if (window == nullptr) {
      std::fprintf(stderr, "simulate: window: %s\n", SDL_GetError());
      return 1;
    }
    s_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (s_renderer == nullptr) {
      s_renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (s_renderer == nullptr) {
      std::fprintf(stderr, "simulate: renderer: %s\n", SDL_GetError());
      return 1;
    }
    s_texture = SDL_CreateTexture(s_renderer, pixelFormat(),
                                  SDL_TEXTUREACCESS_STREAMING, SIM_W, SIM_H);
    if (s_texture == nullptr) {
      std::fprintf(stderr, "simulate: texture: %s\n", SDL_GetError());
      return 1;
    }
  }

  lv_init();
  lv_tick_set_cb(tickMs);
  createDisplay();

  ThetaGP::Ui::Menu::init();
  ThetaGP::Ui::Menu::setLink(false);
  ThetaGP::Ui::Menu::setProfile(1, 2, "Main");
  ThetaGP::Ui::Menu::setReportHz(8000);
  lv_refr_now(NULL);

  if (dump != nullptr) {
    const bool ok = dumpPpm(dump);
    if (s_texture != nullptr) {
      SDL_DestroyTexture(s_texture);
      SDL_DestroyRenderer(s_renderer);
      SDL_Quit();
    }
    return ok ? 0 : 1;
  }

  for (;;) {
    handleKeys();
    if (s_quit) {
      break;
    }
    uint32_t idle = lv_timer_handler();
    if (idle < 2) {
      idle = 2;
    }
    if (idle > 20) {
      idle = 20;
    }
    SDL_Delay(idle);
  }

  SDL_DestroyTexture(s_texture);
  SDL_DestroyRenderer(s_renderer);
  SDL_Quit();
  return 0;
}
