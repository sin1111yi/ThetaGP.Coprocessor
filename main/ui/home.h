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

// The main page: the names on the left, the rail in the middle, the chosen
// page's parameters on the right and the link's names at the foot of that
// column. The pages themselves belong to the menu that shows them.

#pragma once

#include "lvgl.h"

namespace ThetaGP::Ui::Home {

// Starts the timer that turns the link line. Called once, when the menu is
// first built.
void begin();

// Fills `root` with the page. `free()` runs before the root is cleaned: the
// link line's pixels live outside LVGL's own heap.
void build(lv_obj_t *root);
void free(void);

// Places the names for the place the column has settled on, and refills the
// parameters from the model.
void layout(void);

// Repaints the link line for the model as it now stands; `refresh` also
// refills the parameters, which print the link.
void refresh(void);
void showLink(void);

// The place the column has settled on, and the two ways it moves: one place
// with the spring, or straight there.
int selected(void);
void step(int step);

} // namespace ThetaGP::Ui::Home
