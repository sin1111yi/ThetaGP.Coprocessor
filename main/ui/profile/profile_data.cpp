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

#include "ui/profile/profile_data.h"

#include <cstdio>

#include "ui/model.h"

namespace ThetaGP::Ui::Profile::Data {

namespace {

// The lines the right column carries: the profile's own menu.
const char *const kMenuNames[kMenuRows] = {"NAME", "ACTIVE", "MAPPING CONFIG",
                                           "OPTIONS", "LED EFFECTS"};

// The settings the window lists, and the values each one offers. The values
// are the build's own for now: writing one back is the link's work.
const char *const kSettings[kSettingRows] = {"usb.input_mode", "led.bri",
                                             "map.socd", "usb.wired_report_hz"};
const char *const kValues[kSettingRows][4] = {
    {"button", "gamepad", "", ""},
    {"0", "25", "50", "100"},
    {"off", "up", "down", ""},
    {"1000", "2000", "4000", "8000"},
};

uint8_t s_valuePick[kSettingRows] = {0, 2, 0, 3};

// The profile the mark opened the menu for: the name of the card it sits on.
void profileName(uint8_t name, char *out, size_t cap) {
  nameLabel(name, out, cap);
}

// One line of the profile's menu: the name it carries and its value.
void menuLine(uint8_t name, int row, char label[40], char value[40]) {
  snprintf(label, 40, "%s", kMenuNames[row]);
  if (isPlus(name)) {
    // A card that stands for the profile not made yet has nothing to say.
    snprintf(value, 40, "none");
    return;
  }
  switch (row) {
  case 0: {
    char called[26] = {};
    profileName(name, called, sizeof called);
    snprintf(value, 40, "%s", called);
    break;
  }
  case 1:
    snprintf(value, 40, "%s", isActive(name) ? "active" : "stored");
    break;
  default:
    snprintf(value, 40, "%s", isFactory(name) ? "default" : ">");
    break;
  }
}

} // namespace

uint8_t profileCount(void) {
  const uint8_t count = (uint8_t)model().profile_count;
  return count > kNamesMax ? kNamesMax : count;
}

bool isFactory(uint8_t index) { return index == 0; }

bool isPlus(uint8_t index) { return index == (uint8_t)(profileCount() + 1); }

uint8_t nameCount(void) { return (uint8_t)(profileCount() + 2); }

void nameLabel(uint8_t index, char *out, size_t cap) {
  const Model &m = model();
  if (isFactory(index)) {
    snprintf(out, cap, "%s", "Default");
    return;
  }
  if (isPlus(index)) {
    snprintf(out, cap, "%s", "+");
    return;
  }
  if (index == m.profile_id) {
    snprintf(out, cap, "%s", m.profile_name);
  } else {
    snprintf(out, cap, "PROFILE %u", (unsigned)index);
  }
}

bool isActive(uint8_t index) {
  return !isPlus(index) && index == model().profile_id;
}

void menuBlock(uint8_t name, char out[kRows][2][40], int top) {
  for (int i = 0; i < kRows; ++i) {
    const int row = top + i;
    if (row >= 0 && row < kMenuRows) {
      menuLine(name, row, out[i][0], out[i][1]);
    } else {
      out[i][0][0] = '\0';
      out[i][1][0] = '\0';
    }
  }
}

void listBlock(bool values, uint8_t setting, uint8_t valueRow,
               char out[kRows][2][40], int top) {
  for (int i = 0; i < kRows; ++i) {
    out[i][0][0] = '\0';
    out[i][1][0] = '\0';
    const int row = top + i;
    if (!values) {
      if (row < kSettingRows) {
        snprintf(out[i][0], 40, "%s", kSettings[row]);
        snprintf(out[i][1], 40, "%s",
                 kValues[row][s_valuePick[row] < 4 ? s_valuePick[row] : 0]);
      }
    } else {
      if (row < 4) {
        snprintf(out[i][1], 40, "%s", kValues[setting][row]);
      }
    }
  }
}

const char *settingName(uint8_t setting) {
  return setting < kSettingRows ? kSettings[setting] : kSettings[0];
}

uint8_t valueCount(uint8_t setting) {
  uint8_t count = 0;
  if (setting >= kSettingRows) {
    return 0;
  }
  while (count < 4 && kValues[setting][count][0] != '\0') {
    ++count;
  }
  return count;
}

uint8_t valuePick(uint8_t setting) {
  return setting < kSettingRows ? s_valuePick[setting] : 0;
}

void setValuePick(uint8_t setting, uint8_t value) {
  if (setting < kSettingRows) {
    s_valuePick[setting] = value;
  }
}

} // namespace ThetaGP::Ui::Profile::Data
