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

#include "ui/profile.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ui/menu.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Profile {

using Theme::colour;
using Theme::fill;
using Theme::surface;
using Theme::text;

// The frame the menu already has: the names along the arc, the rail beside
// them and the right column's four lines.
#define CARD_W      156
#define CARD_H      38
#define CARD_PITCH  40
#define CARD_CENTER 70
#define CARD_CURVE  22
#define CARD_VIS    1.35f
#define CARD_MAX    8
#define RAIL_X      176
#define RAIL_Y      12
#define RAIL_H      116
#define THUMB_H     20
#define RIGHT_X     200
#define RIGHT_W     214
#define LINE_H      18
#define RIGHT_ROWS  4
#define RIGHT_Y     ((140 - LINE_H * RIGHT_ROWS) / 2)
#define MOVE_MS     480

// The window a setting is edited in: smaller than the screen, over the middle
// of it.
#define POP_W       320
#define POP_H       104
#define POP_X       ((420 - POP_W) / 2)
#define POP_Y       ((140 - POP_H) / 2)

enum Focus : uint8_t { kNames, kMenu, kSettings, kValue };

// The lines the right column carries: the profile's own menu.
#define MENU_ROWS 5
static const char *const MENU_NAMES[MENU_ROWS] = {
    "NAME", "ACTIVE", "MAPPING CONFIG", "OPTIONS", "LED EFFECTS"};

// The settings the window lists, and the values each one offers. The values
// are the build's own for now: writing one back is the link's work.
#define SETTING_ROWS 4
static const char *const SETTINGS[SETTING_ROWS] = {
    "usb.input_mode", "led.bri", "map.socd", "usb.wired_report_hz"};
static const char *const VALUES[SETTING_ROWS][4] = {
    {"button", "gamepad", "", ""},
    {"0", "25", "50", "100"},
    {"off", "up", "down", ""},
    {"1000", "2000", "4000", "8000"},
};
static uint8_t s_valuePick[SETTING_ROWS] = {0, 2, 0, 3};

static Focus s_focus;

// The names: where the column is, which card the mark is on, and the cards.
static float s_scroll;
static float s_animFrom;
static float s_animDiff;
static int s_name;
static uint8_t s_nameCount;
static lv_obj_t *s_cards[CARD_MAX];
static lv_obj_t *s_titles[CARD_MAX];
static lv_obj_t *s_thumb;

// The right column: which line the mark is on, and how far the four-line
// window has travelled into the five.
static int s_row;
static int s_offset;

// The window's own mark.
static int s_setting;
static int s_valueRow;

static lv_obj_t *s_root;
static lv_obj_t *s_right; // the window the right column is seen through
static lv_obj_t *s_rightCanvas;
static uint8_t *s_rightPixels;
static int s_rightY;

static lv_obj_t *s_popup; // the window a setting is edited in
static lv_obj_t *s_popupTitle;
static lv_obj_t *s_popupRow[RIGHT_ROWS];
static lv_obj_t *s_popupName[RIGHT_ROWS];
static lv_obj_t *s_popupMark[RIGHT_ROWS];

static void paint_right(const char lines[RIGHT_ROWS][2][40], int focusRow);
static void popupShow(void);

// ── what the level carries ──

static uint8_t nameCount(void) {
  const uint8_t count = (uint8_t)model().profile_count;
  if (count == 0) {
    return 1;
  }
  return count > CARD_MAX ? CARD_MAX : count;
}

static void nameLabel(uint8_t index, char *out, size_t cap) {
  const Model &m = model();
  if (index + 1 == m.profile_id) {
    snprintf(out, cap, "%s", m.profile_name);
  } else {
    snprintf(out, cap, "PROFILE %u", (unsigned)(index + 1));
  }
}

static bool isActive(uint8_t index) { return index + 1 == model().profile_id; }

// The profile the mark opened the menu for: the name of the card it sits on.
static void profileName(char *out, size_t cap) {
  nameLabel((uint8_t)s_name, out, cap);
}

// One line of the profile's menu: the name it carries and its value.
static void menuLine(int row, char label[40], char value[40]) {
  snprintf(label, 40, "%s", MENU_NAMES[row]);
  switch (row) {
  case 0: {
    char name[26] = {};
    profileName(name, sizeof name);
    snprintf(value, 40, "%s", name);
    break;
  }
  case 1:
    snprintf(value, 40, "%s", isActive((uint8_t)s_name) ? "active" : "stored");
    break;
  default:
    snprintf(value, 40, ">");
    break;
  }
}

// The four lines of the menu the window is showing, in order.
static void menuBlock(char out[RIGHT_ROWS][2][40], int top) {
  for (int i = 0; i < RIGHT_ROWS; ++i) {
    const int row = top + i;
    if (row >= 0 && row < MENU_ROWS) {
      menuLine(row, out[i][0], out[i][1]);
    } else {
      out[i][0][0] = '\0';
      out[i][1][0] = '\0';
    }
  }
}

// The four lines of a settings list, either the list of them or the values one
// of them offers.
static void listBlock(char out[RIGHT_ROWS][2][40], bool values, int top) {
  for (int i = 0; i < RIGHT_ROWS; ++i) {
    out[i][0][0] = '\0';
    out[i][1][0] = '\0';
    const int row = top + i;
    if (!values) {
      if (row < SETTING_ROWS) {
        snprintf(out[i][0], 40, "%s", SETTINGS[row]);
        snprintf(out[i][1], 40, "%s",
                 VALUES[row][s_valuePick[row] < 4 ? s_valuePick[row] : 0]);
      }
    } else {
      if (row < 4) {
        const char *value = VALUES[s_setting][row];
        if (value[0] != '\0') {
          snprintf(out[i][0], 40, "%s", row == s_valueRow ? "*" : "");
          snprintf(out[i][1], 40, "%s", value);
        }
      }
    }
  }
}

// ── the right column ──

static void paint_right(const char lines[RIGHT_ROWS][2][40], int focusRow) {
  if (s_rightCanvas == NULL) {
    return;
  }

  lv_canvas_fill_bg(s_rightCanvas, colour(Theme::BG), LV_OPA_COVER);
  lv_layer_t layer;
  lv_canvas_init_layer(s_rightCanvas, &layer);

  for (int i = 0; i < RIGHT_ROWS; ++i) {
    if (lines[i][0][0] == '\0') {
      continue;
    }
    const lv_coord_t top = (lv_coord_t)(i * LINE_H);
    const bool on = i == focusRow;

    lv_draw_rect_dsc_t rule;
    lv_draw_rect_dsc_init(&rule);
    rule.bg_color = colour(Theme::BORDER);
    rule.bg_opa = LV_OPA_COVER;
    lv_area_t ruleArea = {0, top, (lv_coord_t)(RIGHT_W - 1), top};
    lv_draw_rect(&layer, &rule, &ruleArea);

    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.font = &lv_font_montserrat_12;
    label.color = colour(on ? Theme::ACCENT : Theme::MUTED);
    label.text = lines[i][0];
    label.text_length = (uint32_t)std::strlen(lines[i][0]);
    lv_area_t labelArea = {0, (lv_coord_t)(top + 2), (lv_coord_t)(RIGHT_W - 1),
                           (lv_coord_t)(top + 15)};
    lv_draw_label(&layer, &label, &labelArea);

    lv_draw_label_dsc_t value;
    lv_draw_label_dsc_init(&value);
    value.font = &lv_font_montserrat_14;
    value.color = colour(on ? Theme::ACCENT : Theme::TEXT);
    value.text = lines[i][1];
    value.text_length = (uint32_t)std::strlen(lines[i][1]);
    value.align = LV_TEXT_ALIGN_RIGHT;
    lv_area_t valueArea = {0, (lv_coord_t)(top + 1), (lv_coord_t)(RIGHT_W - 1),
                           (lv_coord_t)(top + 17)};
    lv_draw_label(&layer, &value, &valueArea);
  }

  lv_canvas_finish_layer(s_rightCanvas, &layer);
}

// What the right column says, for the state the mark is in.
static void rightShow(void) {
  char lines[RIGHT_ROWS][2][40] = {};
  int focus = -1;
  switch (s_focus) {
  case kMenu:
    menuBlock(lines, s_offset);
    focus = s_row - s_offset;
    break;
  case kSettings:
    listBlock(lines, false, s_offset);
    focus = s_setting - s_offset;
    break;
  case kValue:
    listBlock(lines, true, s_offset);
    focus = s_valueRow - s_offset;
    break;
  default: {
    // The names have the focus: the column says what the card under it is.
    char name[26] = {};
    profileName(name, sizeof name);
    snprintf(lines[0][0], 40, "NAME");
    snprintf(lines[0][1], 40, "%s", name);
    snprintf(lines[1][0], 40, "ACTIVE");
    snprintf(lines[1][1], 40, "%s", isActive((uint8_t)s_name) ? "yes" : "no");
    snprintf(lines[2][0], 40, "MAPPING CONFIG");
    snprintf(lines[2][1], 40, ">");
    snprintf(lines[3][0], 40, "OPTIONS");
    snprintf(lines[3][1], 40, ">");
    break;
  }
  }
  paint_right(lines, focus);
  s_rightY = 0;
  if (s_rightCanvas != NULL) {
    lv_obj_set_y(s_rightCanvas, 0);
  }
}

// ── the cards ──

static float rel(int i) {
  float away = (float)i - s_scroll;
  while (away > s_nameCount / 2.0f) {
    away -= s_nameCount;
  }
  while (away < -s_nameCount / 2.0f) {
    away += s_nameCount;
  }
  return away;
}

static void cardLayout(void) {
  int chosen = (int)std::floor(s_scroll + 0.5f);
  chosen = ((chosen % s_nameCount) + s_nameCount) % s_nameCount;

  for (int i = 0; i < s_nameCount; ++i) {
    const float away = rel(i);
    const float far = away < 0 ? -away : away;
    lv_obj_t *card = s_cards[i];
    if (far > CARD_VIS) {
      lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
      continue;
    }
    lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);

    const int bend = (int)(CARD_CURVE * away * away);
    lv_obj_set_pos(card, 6 - bend,
                   CARD_CENTER - CARD_H / 2 + (int)(away * CARD_PITCH));
    lv_obj_set_style_opa(card, (lv_opa_t)(255 - (int)(95 * far)), 0);

    const bool on = i == chosen;
    fill(card, on ? Theme::SURFACE_HI : Theme::SURFACE);
    lv_obj_set_style_border_width(card, on ? 0 : 1, 0);
    lv_obj_set_style_text_color(s_titles[i],
                                colour(on ? Theme::TEXT : Theme::MUTED), 0);
  }

  if (s_thumb != NULL) {
    float t = s_scroll / (float)(s_nameCount > 1 ? s_nameCount - 1 : 1);
    if (t < 0) {
      t = 0;
    }
    if (t > 1) {
      t = 1;
    }
    lv_obj_set_y(s_thumb, (int)(t * (RAIL_H - THUMB_H)));
  }
}

static int32_t spring(const lv_anim_t *anim) {
  const float t = (float)anim->act_time / (float)anim->duration;
  const float settle =
      1.0f -
      std::exp(-6.0f * t) * (std::cos(9.0f * t) + 0.67f * std::sin(9.0f * t));
  return anim->start_value +
         (int32_t)((float)(anim->end_value - anim->start_value) * settle);
}

static void cardAnim(void *var, int32_t value) {
  (void)var;
  const float t = (float)value / 1000.0f;
  s_scroll = s_animFrom + s_animDiff * t;
  cardLayout();
}

static void cardAnimDone(lv_anim_t *anim) {
  (void)anim;
  float pos = s_scroll;
  pos -= std::floor(pos / s_nameCount) * s_nameCount;
  s_name = (int)std::floor(pos + 0.5f) % s_nameCount;
  s_scroll = (float)s_name;
  cardLayout();
  rightShow();
}

// ── the level ──

void build(lv_obj_t *root) {
  s_root = root;
  s_nameCount = nameCount();
  if (s_name >= s_nameCount) {
    s_name = 0;
  }
  s_scroll = (float)s_name;
  s_offset = 0;
  s_rightY = 0;

  for (int i = 0; i < s_nameCount; ++i) {
    lv_obj_t *card = surface(root, 0, 0, CARD_W, CARD_H);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    char name[26] = {};
    nameLabel((uint8_t)i, name, sizeof name);
    s_titles[i] = text(card, name, &lv_font_montserrat_14, Theme::TEXT, 14, 11,
                       CARD_W - 28);
    s_cards[i] = card;
  }

  lv_obj_t *track = lv_obj_create(root);
  lv_obj_remove_style_all(track);
  lv_obj_set_pos(track, RAIL_X, RAIL_Y);
  lv_obj_set_size(track, 2, RAIL_H);
  lv_obj_set_style_radius(track, 1, 0);
  fill(track, 0x3A2A1F);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

  s_thumb = lv_obj_create(track);
  lv_obj_remove_style_all(s_thumb);
  lv_obj_set_pos(s_thumb, 0, 0);
  lv_obj_set_size(s_thumb, 2, THUMB_H);
  lv_obj_set_style_radius(s_thumb, 1, 0);
  fill(s_thumb, 0xFDBA74);

  s_right = lv_obj_create(root);
  lv_obj_remove_style_all(s_right);
  lv_obj_set_pos(s_right, RIGHT_X, RIGHT_Y);
  lv_obj_set_size(s_right, RIGHT_W, LINE_H * RIGHT_ROWS);
  lv_obj_set_style_bg_opa(s_right, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(s_right, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_right, LV_OBJ_FLAG_SCROLLABLE);

  s_rightPixels =
      (uint8_t *)std::malloc((size_t)RIGHT_W * LINE_H * RIGHT_ROWS * 2);
  if (s_rightPixels != NULL) {
    s_rightCanvas = lv_canvas_create(s_right);
    lv_obj_remove_style_all(s_rightCanvas);
    lv_canvas_set_buffer(s_rightCanvas, s_rightPixels, RIGHT_W,
                         LINE_H * RIGHT_ROWS, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_rightCanvas, 0, 0);
    lv_obj_remove_flag(s_rightCanvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_rightCanvas, LV_OBJ_FLAG_SCROLLABLE);
  }

  cardLayout();
  rightShow();
}

void free(void) {
  lv_anim_delete(&s_scroll, NULL);
  if (s_popup != NULL) {
    lv_obj_delete(s_popup);
    s_popup = NULL;
  }
  if (s_rightCanvas != NULL) {
    lv_obj_delete(s_rightCanvas);
    s_rightCanvas = NULL;
  }
  if (s_rightPixels != NULL) {
    std::free(s_rightPixels);
    s_rightPixels = NULL;
  }
  s_root = NULL;
  s_right = NULL;
  s_thumb = NULL;
}

void layout(void) {
  cardLayout();
  rightShow();
  if (s_popup != NULL) {
    lv_obj_move_foreground(s_popup);
  }
}

void move(int step) {
  switch (s_focus) {
  case kNames: {
    lv_anim_delete(&s_scroll, NULL);
    s_animFrom = s_scroll;
    s_animDiff = (float)step;
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, &s_scroll);
    lv_anim_set_values(&anim, 0, 1000);
    lv_anim_set_exec_cb(&anim, cardAnim);
    lv_anim_set_completed_cb(&anim, cardAnimDone);
    lv_anim_set_path_cb(&anim, spring);
    lv_anim_set_duration(&anim, MOVE_MS);
    lv_anim_start(&anim);
    break;
  }
  case kMenu: {
    int row = s_row + step;
    if (row < 0) {
      row = 0;
    }
    if (row >= MENU_ROWS) {
      row = MENU_ROWS - 1;
    }
    s_row = row;
    // The four-line window follows the mark once it would leave it.
    if (s_row < s_offset) {
      s_offset = s_row;
    }
    if (s_row >= s_offset + RIGHT_ROWS) {
      s_offset = s_row - RIGHT_ROWS + 1;
    }
    rightShow();
    break;
  }
  case kSettings: {
    int row = s_setting + step;
    if (row < 0) {
      row = SETTING_ROWS - 1;
    }
    if (row >= SETTING_ROWS) {
      row = 0;
    }
    s_setting = row;
    if (s_setting < s_offset) {
      s_offset = s_setting;
    }
    if (s_setting >= s_offset + RIGHT_ROWS) {
      s_offset = s_setting - RIGHT_ROWS + 1;
    }
    popupShow();
    break;
  }
  case kValue: {
    int count = 0;
    while (count < 4 && VALUES[s_setting][count][0] != '\0') {
      ++count;
    }
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

// ── the window over the middle ──

// Draws the window for the state the mark is in: the settings of the profile,
// or the values one of them offers.
static void popupShow(void) {
  if (s_popup == NULL) {
    return;
  }

  if (s_focus == kValue) {
    lv_label_set_text(s_popupTitle, SETTINGS[s_setting]);
  } else {
    lv_label_set_text(s_popupTitle, "OPTIONS");
  }

  char lines[RIGHT_ROWS][2][40] = {};
  listBlock(lines, s_focus == kValue, s_offset);
  const bool values = s_focus == kValue;

  for (int i = 0; i < RIGHT_ROWS; ++i) {
    const bool used = lines[i][1][0] != '\0';
    const int row = s_offset + i;
    const int mark = values ? s_valueRow : s_setting;
    const bool on = used && row == mark;

    // The name sits at the left and the value at the right of the window; a
    // list of values has no name column, so the value takes the whole line.
    // A list of values takes the whole line; a list of settings keeps the
    // right hundred pixels for the value it holds.
    lv_obj_set_x(s_popupRow[i], values ? 26 : (lv_coord_t)(POP_W - 112));
    lv_obj_set_width(s_popupRow[i], values ? (lv_coord_t)(POP_W - 38) : 100);
    lv_obj_set_style_text_align(
        s_popupRow[i], values ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_popupRow[i], used ? lines[i][1] : "");
    lv_obj_set_style_text_color(s_popupRow[i],
                                colour(on ? Theme::ACCENT : Theme::TEXT), 0);

    lv_label_set_text(s_popupName[i], values ? "" : lines[i][0]);
    lv_obj_set_style_text_color(s_popupName[i],
                                colour(on ? Theme::ACCENT : Theme::MUTED), 0);

    lv_label_set_text(s_popupMark[i], on ? ">" : "");
    lv_obj_set_style_text_color(s_popupMark[i],
                                colour(on ? Theme::ACCENT : Theme::MUTED), 0);

    lv_obj_t *parts[3] = {s_popupRow[i], s_popupName[i], s_popupMark[i]};
    for (lv_obj_t *part : parts) {
      if (used) {
        lv_obj_remove_flag(part, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

// Opens the window on the settings of the profile the menu is showing.
static void popupOpen(void) {
  if (s_popup != NULL) {
    return;
  }

  s_popup = lv_obj_create(s_root);
  lv_obj_remove_style_all(s_popup);
  lv_obj_set_pos(s_popup, POP_X, POP_Y);
  lv_obj_set_size(s_popup, POP_W, POP_H);
  fill(s_popup, Theme::SURFACE);
  lv_obj_set_style_radius(s_popup, 10, 0);
  lv_obj_set_style_border_color(s_popup, colour(Theme::BORDER), 0);
  lv_obj_set_style_border_width(s_popup, 1, 0);
  lv_obj_remove_flag(s_popup, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);

  s_popupTitle = text(s_popup, "OPTIONS", &lv_font_montserrat_12, Theme::MUTED,
                      12, 6, POP_W - 24);
  for (int i = 0; i < RIGHT_ROWS; ++i) {
    s_popupMark[i] = text(s_popup, "", &lv_font_montserrat_12, Theme::MUTED, 12,
                          26 + i * LINE_H, 10);
    s_popupName[i] = text(s_popup, "", &lv_font_montserrat_12, Theme::MUTED, 26,
                          26 + i * LINE_H, 170);
    s_popupRow[i] = text(s_popup, "", &lv_font_montserrat_14, Theme::TEXT, 26,
                         25 + i * LINE_H, POP_W - 40);
  }

  s_setting = 0;
  s_offset = 0;
  s_focus = kSettings;
  popupShow();
}

static void popupClose(void) {
  if (s_popup != NULL) {
    lv_obj_delete(s_popup);
    s_popup = NULL;
  }
}

// ── the mark ──

void enter(void) {
  switch (s_focus) {
  case kNames:
    // The focus leaves the names for the profile's own menu.
    s_focus = kMenu;
    s_row = 0;
    s_offset = 0;
    rightShow();
    break;
  case kMenu:
    // OPTIONS opens the window; the other lines have nothing behind them yet.
    if (s_row == 3) {
      popupOpen();
    }
    break;
  case kSettings:
    s_valueRow = s_valuePick[s_setting] < 4 ? s_valuePick[s_setting] : 0;
    s_offset = 0;
    s_focus = kValue;
    popupShow();
    break;
  case kValue:
    s_valuePick[s_setting] = (uint8_t)s_valueRow;
    s_offset = 0;
    s_focus = kSettings;
    popupShow();
    break;
  }
}

bool leave(void) {
  switch (s_focus) {
  case kValue:
    s_offset = 0;
    s_focus = kSettings;
    popupShow();
    return true;
  case kSettings:
    popupClose();
    s_focus = kMenu;
    s_offset = 0;
    if (s_row >= RIGHT_ROWS) {
      s_offset = s_row - RIGHT_ROWS + 1;
    }
    rightShow();
    return true;
  case kMenu:
    s_focus = kNames;
    rightShow();
    return true;
  default:
    return false;
  }
}

uint8_t item(void) { return (uint8_t)(s_focus == kNames ? s_name : s_row); }

} // namespace ThetaGP::Ui::Profile
