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

// The menu the panel shows: a column of cards, an arc that tracks it and the
// pages behind them. It owns no state the protocol cannot fill: the model is
// what `ThetaGP.PB` will write and what the pages read.

#pragma once

#include <cstdint>

#include "lvgl.h"

namespace ThetaGP::Ui {

// What the panel shows about the device, filled from the protocol messages the
// coprocessor receives.
struct Model {
  const char *board = "ThetaGP-H743";
  const char *profile_name = "Main";
  uint32_t profile_id = 1;
  uint32_t profile_count = 2;
  uint32_t report_hz = 8000;
  bool link = false;
};

const Model &model();

namespace Menu {

// Creates the screen on the active display, at the top of the menu.
void init();

// The link state the status page shows: a live protocol link or none.
void setLink(bool up);

// The device's report rate, as sys.get_usage reports it.
void setReportHz(uint32_t hz);

// The active profile's name and id, as profile.list reports them.
void setProfile(uint32_t id, uint32_t count, const char *name);

// Navigation, driven by the link's buttons or, until it is up, by the board
// button: a short press moves the mark, a long press enters or leaves a page.
void next();
void enter();
void back();

} // namespace Menu

} // namespace ThetaGP::Ui
