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

// The pages the menu opens: one file draws one page, each of them reading the
// model and the mark it is handed and touching nothing else.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ThetaGP::Ui::Pages {

struct Page;

// One line of a page that lists its items instead of drawing them.
struct Item {
  const char *label;
  const Page *page;
};

// What a page draws: its body, and where the mark sits in it.
using Draw = void (*)(lv_obj_t *parent, uint8_t selected);

struct Page {
  const char *title;
  // Draws the page's body; a page without one lists its items.
  Draw draw;
  const Item *items;
  uint8_t count;
};

// The rows a page lists when it draws nothing of its own: a surface per item,
// the chosen one filled and carrying the accent mark.
void drawItems(lv_obj_t *parent, const Item *items, uint8_t count,
               uint8_t selected);

void drawMonitor(lv_obj_t *parent, uint8_t selected);
void drawOptions(lv_obj_t *parent, uint8_t selected);
void drawMapping(lv_obj_t *parent, uint8_t selected);
void drawProfiles(lv_obj_t *parent, uint8_t selected);
void drawAbout(lv_obj_t *parent, uint8_t selected);

// The name the profile list carries, which is also the name a profile takes
// when its row is opened.
const char *profileName(uint8_t index);

} // namespace ThetaGP::Ui::Pages
