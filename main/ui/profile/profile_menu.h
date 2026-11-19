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

// The right column of the profile level: the menu the profile carries, one
// line a card, the line the mark is on drawn as the live one.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ThetaGP::Ui::Profile::Menu {

// Builds the column under `parent`.
void build(lv_obj_t *parent);

// Lets go of the objects; the parent's clean deletes them.
void drop(void);

// Puts the column at rest on `name`'s menu, from line `top`, with `mark` on
// the line the mark is on (-1 for none).
void show(uint8_t name, int top, int mark);

// Slides the column one card for the same content, in the direction the mark
// moved.
void slide(uint8_t name, int top, int mark, int step);

} // namespace ThetaGP::Ui::Profile::Menu
