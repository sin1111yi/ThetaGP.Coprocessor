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

// What the profile level says: the names the store carries, the profile's own
// menu, and the settings a profile holds. Text only -- nothing here holds an
// object.

#pragma once

#include <cstddef>
#include <cstdint>

namespace ThetaGP::Ui::Profile::Data {

// The lines one window carries, and the menu's own count.
inline constexpr int kRows = 4;
inline constexpr int kMenuRows = 5;
inline constexpr int kMenuOptions = 3;
inline constexpr int kSettingRows = 4;

// The most names the level carries: the factory profile, the store's, and the
// plus a profile is made from.
inline constexpr int kNamesMax = 6;

// The list the store carries: the factory profile first, then one card per
// profile, then the plus a profile is made from. The factory profile is the
// build's own -- it is read and never written.
uint8_t profileCount(void);
bool isFactory(uint8_t index);
bool isPlus(uint8_t index);
uint8_t nameCount(void);
void nameLabel(uint8_t index, char *out, size_t cap);
bool isActive(uint8_t index);

// The lines the column carries, for the profile the menu is open on.
void menuBlock(uint8_t name, char out[kRows][2][40], int top);

// The lines a settings list carries: the key with the value it holds, or the
// values one of them offers.
void listBlock(bool values, uint8_t setting, uint8_t valueRow,
               char out[kRows][2][40], int top);

// The setting a window line belongs to, and the values it offers.
const char *settingName(uint8_t setting);
uint8_t valueCount(uint8_t setting);

// Which value a setting currently holds.
uint8_t valuePick(uint8_t setting);
void setValuePick(uint8_t setting, uint8_t value);

} // namespace ThetaGP::Ui::Profile::Data
