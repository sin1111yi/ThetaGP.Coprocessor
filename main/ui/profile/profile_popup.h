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

// The window a profile setting is chosen in: the settings of the profile, or
// the values one of them offers, floating over the level behind it.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ThetaGP::Ui::Profile::Popup {

// Builds the window over `parent`. It draws nothing until `show` is called.
void open(lv_obj_t *parent);

bool isOpen(void);

// Draws the lines for the state the mark is in: the settings of the profile,
// or the values one setting offers.
void show(bool values, uint8_t setting, uint8_t valueRow);

// Keeps the window above what the level draws after it.
void foreground(void);

void close(void);

} // namespace ThetaGP::Ui::Profile::Popup
