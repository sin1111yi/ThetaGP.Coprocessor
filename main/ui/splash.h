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

// The wordmark that plays over the menu once at power-up: it fades in, holds,
// then fades out and uncovers what is behind it.

#pragma once

#include "lvgl.h"

namespace ThetaGP::Ui::Splash {

// Covers `root` with the wordmark and starts it.
void show(lv_obj_t *root);

// Ends the wordmark now, for a page rebuilt under it: the fade callbacks must
// never touch an object the clean is about to delete.
void drop(void);

// True once the wordmark has played out (or was dropped).
bool done(void);

} // namespace ThetaGP::Ui::Splash
