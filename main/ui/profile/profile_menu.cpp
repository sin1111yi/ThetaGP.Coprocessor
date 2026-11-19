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

#include "ui/profile/profile_menu.h"

#include "ui/profile/profile_data.h"
#include "ui/widgets/column.h"

namespace ThetaGP::Ui::Profile::Menu {

namespace {

// The right column's place in the level.
constexpr int kX = 200;
constexpr int kWidth = 215;
constexpr int kY = (140 - Cards::HEIGHT) / 2;

static_assert(Data::kRows == Cards::ROWS,
              "the column and the window carry the same lines");

Cards::Column s_column;

// Fills the column with the lines the profile's menu carries, the line the
// mark is on drawn as the live one.
void fill(uint8_t name, int top, int mark) {
  char block[Data::kRows][2][40] = {};
  Cards::Line lines[Data::kRows] = {};
  Data::menuBlock(name, block, top);

  for (int i = 0; i < Data::kRows; ++i) {
    if (block[i][1][0] == '\0') {
      continue;
    }
    lines[i].label = block[i][0];
    lines[i].value = block[i][1];
  }

  s_column.fill(lines, Data::kRows, 0);
  s_column.focus(mark);
}

} // namespace

void build(lv_obj_t *parent) { s_column.build(parent, kX, kY, kWidth); }

void drop(void) { s_column.drop(); }

void show(uint8_t name, int top, int mark) {
  fill(name, top, mark);
  s_column.rest();
}

void slide(uint8_t name, int top, int mark, int step) {
  fill(name, top, mark);
  s_column.slide(step);
}

} // namespace ThetaGP::Ui::Profile::Menu
