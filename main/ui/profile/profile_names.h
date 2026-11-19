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

// The profile names along the arc: one card a name, a rail beside them and a
// thumb that follows, and the spring that carries the wheel one card.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ThetaGP::Ui::Profile::Names {

// Builds one card per name the level carries, with the wheel at `at`.
void build(lv_obj_t *parent, uint8_t count, int at);

// Lays the cards out where the wheel is, and the thumb with them.
void layout(void);

// Slides one card, in the direction the mark moved; `arrived` is told which
// card it settled on.
void slide(int step, void (*arrived)(uint8_t index));

// Drops a move in flight where it stands.
void stop(void);

// Lets go of the objects; the parent's clean deletes them.
void drop(void);

} // namespace ThetaGP::Ui::Profile::Names
