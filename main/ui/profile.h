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

// The profiles: the names down the left, the profile's own menu down the
// right, and a window over the middle for the settings that are edited in
// place. The frame is the menu's own -- the same cards, the same rail.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ThetaGP::Ui::Profile {

// Fills `root` with the profile level. `free()` runs before the root is
// cleaned: the columns' pixels sit outside LVGL's own heap.
void build(lv_obj_t *root);
void free(void);
void layout(void);

// The mark: a name on the left, or a line of the profile's menu on the right.
// `enter` follows the mark; `leave` goes back one step and reports whether it
// went anywhere, so a caller knows when to leave the level.
void move(int step);
void enter(void);
bool leave(void);

// The place the mark sits on, as the link reports the panel: the row of the
// profile list, or the row of the right column once the focus is there.
uint8_t item(void);

} // namespace ThetaGP::Ui::Profile
