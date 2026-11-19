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

// The menu the panel shows: names on the left, a progress bar in the middle,
// the chosen page's parameters on the right, and the pages behind them. It
// owns no state the protocol cannot fill: the model is what `ThetaGP.PB` will
// write and what the pages read.

#pragma once

#include <cstdint>

#include "lvgl.h"

#include "ui/model.h"

namespace ThetaGP::Ui {

namespace Menu {

// The pages the panel shows, in the order the menu lists them. The numbers are
// the link's own: the schema names the same six, and the protocol layer checks
// that they still agree.
enum class PageId : uint8_t {
  Menu = 0,
  Monitor = 1,
  Options = 2,
  Mapping = 3,
  Profiles = 4,
  About = 5,
};

// What the screen shows: the page, and the place the mark sits on it.
struct Screen {
  PageId page;
  uint8_t item;
};

// Creates the screen on the active display, at the top of the menu. The
// power-up wordmark plays over it, and nothing outside the menu should start
// talking to the main board before it has finished.
void init();

// True once the wordmark has played out (or was skipped).
bool introDone();

// The page on screen and the place the mark sits on it, as the link reports
// the panel.
Screen screen();

// Navigation, driven by the link's buttons or, until it is up, by the board
// button: a short press moves the mark, a long press enters or leaves a page.
void next();
void prev();
void enter();
void back();

} // namespace Menu

} // namespace ThetaGP::Ui
