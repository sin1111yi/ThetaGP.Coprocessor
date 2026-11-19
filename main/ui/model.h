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

// What the panel shows about the device. The protocol writes it, the pages
// read it, and nothing else holds it.

#pragma once

#include <cstdint>

namespace ThetaGP::Ui {

struct Model {
  const char *board = "ThetaGP-H743";
  const char *profile_name = "Main";
  // What the main board reported over the link: its identifier and firmware
  // version, empty until it answers.
  char peer_id[17] = "";
  char peer_version[21] = "";
  uint32_t profile_id = 1;
  uint32_t profile_count = 2;
  uint32_t report_hz = 8000;
  bool link = false;
};

const Model &model();

namespace Menu {

// The link state the status page shows: a live protocol link or none.
void setLink(bool up);

// The device's report rate, as sys.get_usage reports it.
void setReportHz(uint32_t hz);

// The main board's identifier and firmware version, as the link reports them.
void setPeer(const char *id, const char *version);

// The active profile's name and id, as profile.list reports them.
void setProfile(uint32_t id, uint32_t count, const char *name);

// The board the link reports is the board the pages name.
void setBoard(const char *id);

// Rebuilds what is on screen for a value that changed: a setter above calls
// this, and nothing else should.
void refresh();

} // namespace Menu

} // namespace ThetaGP::Ui
