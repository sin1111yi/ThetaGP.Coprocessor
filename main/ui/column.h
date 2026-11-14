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

// The column of cards the pages say their lines in: one line a card, the
// cards five pixels inside the page's edge and their text five pixels inside
// them, sliding a whole card when the mark moves.

#pragma once

#include "lvgl.h"

namespace ThetaGP::Ui::Cards {

// One line of the column: the name at its left, the value at its right.
struct Line {
  const char *label;
  const char *value;
};

// The cards the column shows at once, the room each one takes and the air
// between two of them, and the height of the window they slide in.
inline constexpr int ROWS = 4;
inline constexpr int CARD = 26;
inline constexpr int PITCH = 30;
inline constexpr int HEIGHT = PITCH * ROWS;

class Column {
public:
  // Builds the window and its cards under `root`, `width` wide.
  void build(lv_obj_t *root, int x, int y, int width);

  // Forgets the objects; the root's own clean deletes them.
  void drop(void);

  // Fills the cards from `lines`, starting at `first`. A line the list does
  // not carry is left empty.
  void fill(const Line *lines, int count, int first);

  // Draws one line of the column as the one the mark is on; -1 for none.
  void focus(int row);

  // Puts the column at rest, for a value that changed under the mark.
  void rest(void);

  // Slides the column one card, in the direction the mark moved.
  void slide(int direction);

private:
  lv_obj_t *_window = nullptr;
  lv_obj_t *_list = nullptr;
  lv_obj_t *_card[ROWS] = {};
  lv_obj_t *_name[ROWS] = {};
  lv_obj_t *_value[ROWS] = {};
};

} // namespace ThetaGP::Ui::Cards
