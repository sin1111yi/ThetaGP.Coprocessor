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

#include "ui/profile/profile.h"

#include "ui/profile/profile_data.h"
#include "ui/profile/profile_menu.h"
#include "ui/profile/profile_names.h"
#include "ui/profile/profile_popup.h"

namespace ThetaGP::Ui::Profile {

namespace {

constexpr int kRightRows = Data::kRows;

enum Focus : uint8_t { kNames, kMenu, kSettings, kValue };

Focus s_focus;

// Which card the mark sits on.
int s_name;
uint8_t s_nameCount;

// The right column: which line the mark is on, and how far the four-line
// window has travelled into the five.
int s_row;
int s_offset;

// The window's own mark.
int s_setting;
int s_valueRow;

lv_obj_t *s_root;

// ── the right column ──

// The top line of the menu the column shows: where the mark has scrolled it,
// and no further than the line a window is open on.
int rightTop(void) {
  int top = s_offset;
  if (top > Data::kMenuOptions) {
    top = Data::kMenuOptions;
  }
  return top;
}

// Which of those lines the mark is on; -1 while it is on the names.
int rightMark(void) {
  switch (s_focus) {
  case kMenu:
    return s_row - rightTop();
  case kSettings:
  case kValue:
    // The window is open on this line; the list it holds marks itself.
    return Data::kMenuOptions - rightTop();
  default:
    return -1;
  }
}

// Puts the column at rest on the window the offset names.
void rightShow(void) { Menu::show((uint8_t)s_name, rightTop(), rightMark()); }

// Slides the column one card, in the direction the mark moved.
void rightScroll(int step) {
  Menu::slide((uint8_t)s_name, rightTop(), rightMark(), step);
}

// ── the window ──

void popupShow(void) {
  Popup::show(s_focus == kValue, (uint8_t)s_setting, (uint8_t)s_valueRow);
}

// Opens the window on the settings of the profile the menu is showing.
void openPopup(void) {
  if (Popup::isOpen()) {
    return;
  }
  Popup::open(s_root);
  s_setting = 0;
  s_focus = kSettings;
  popupShow();
  rightShow();
}

// ── the level ──

} // namespace

void build(lv_obj_t *root) {
  s_root = root;
  s_nameCount = Data::nameCount();
  if (s_name >= s_nameCount) {
    s_name = 0;
  }
  s_offset = 0;

  Names::build(root, s_nameCount, s_name);
  Menu::build(root);

  // A page rebuilt under an open window opens it again: the mark keeps the
  // state it was left in.
  if (s_focus == kValue) {
    s_focus = kSettings;
  }
  if (s_focus == kSettings) {
    openPopup();
  } else {
    rightShow();
  }
}

void free(void) {
  Names::stop();
  Popup::close();
  Menu::drop();
  Names::drop();
  s_root = nullptr;
}

void layout(void) {
  Names::layout();
  rightShow();
  Popup::foreground();
}

void move(int step) {
  switch (s_focus) {
  case kNames: {
    const int target = s_name + step;
    if (target < 0 || target >= s_nameCount) {
      return;
    }
    Names::slide(step, [](uint8_t index) {
      s_name = index;
      rightShow();
    });
    break;
  }
  case kMenu: {
    int row = s_row + step;
    if (row < 0) {
      row = 0;
    }
    if (row >= Data::kMenuRows) {
      row = Data::kMenuRows - 1;
    }
    const int was = s_offset;
    s_row = row;
    // The four-line window follows the mark once it would leave it.
    if (s_row < s_offset) {
      s_offset = s_row;
    }
    if (s_row >= s_offset + kRightRows) {
      s_offset = s_row - kRightRows + 1;
    }
    if (s_offset != was) {
      rightScroll(step);
    } else {
      rightShow();
    }
    break;
  }
  case kSettings: {
    int row = s_setting + step;
    if (row < 0) {
      row = Data::kSettingRows - 1;
    }
    if (row >= Data::kSettingRows) {
      row = 0;
    }
    s_setting = row;
    popupShow();
    break;
  }
  case kValue: {
    const int count = (int)Data::valueCount((uint8_t)s_setting);
    int row = s_valueRow + step;
    if (row < 0) {
      row = 0;
    }
    if (row >= count) {
      row = count - 1;
    }
    s_valueRow = row;
    popupShow();
    break;
  }
  }
}

void enter(void) {
  switch (s_focus) {
  case kNames:
    if (Data::isPlus((uint8_t)s_name)) {
      // A profile is made from this card, which is the link's work.
      return;
    }
    if (Data::isFactory((uint8_t)s_name)) {
      // The factory profile is read-only: it opens nothing.
      return;
    }
    // The focus leaves the names for the profile's own menu.
    s_focus = kMenu;
    s_row = 0;
    s_offset = 0;
    rightShow();
    break;
  case kMenu:
    // OPTIONS opens the window; the other lines have nothing behind them yet.
    if (s_row == Data::kMenuOptions) {
      openPopup();
    }
    break;
  case kSettings:
    s_valueRow = Data::valuePick((uint8_t)s_setting);
    s_focus = kValue;
    popupShow();
    break;
  case kValue:
    Data::setValuePick((uint8_t)s_setting, (uint8_t)s_valueRow);
    s_focus = kSettings;
    popupShow();
    break;
  }
}

bool leave(void) {
  switch (s_focus) {
  case kValue:
    s_focus = kSettings;
    popupShow();
    return true;
  case kSettings:
    Popup::close();
    s_focus = kMenu;
    s_offset = 0;
    if (s_row >= kRightRows) {
      s_offset = s_row - kRightRows + 1;
    }
    rightShow();
    return true;
  case kMenu:
    s_focus = kNames;
    s_offset = 0;
    rightShow();
    return true;
  default:
    return false;
  }
}

uint8_t item(void) { return (uint8_t)(s_focus == kNames ? s_name : s_row); }

} // namespace ThetaGP::Ui::Profile
