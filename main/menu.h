#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

// What the panel shows about the device, filled from the protocol messages the
// coprocessor receives.
typedef struct {
  const char *board;
  const char *profile_name;
  uint32_t profile_id;
  uint32_t profile_count;
  uint32_t report_hz;
  bool link;
} menu_model_t;

const menu_model_t *menu_model(void);

// Creates the screen on the active display, at the top of the menu.
void menu_init(void);

// The link state the status page shows: a live protocol link or none.
void menu_set_link(bool up);

// The device's report rate, as sys.get_usage reports it.
void menu_set_report_hz(uint32_t hz);

// The active profile's name and id, as profile.list reports them.
void menu_set_profile(uint32_t id, uint32_t count, const char *name);

// Navigation, driven by the link's buttons or, until it is up, by the board
// button: a short press moves the mark, a long press enters or leaves a page.
void menu_next(void);
void menu_enter(void);
void menu_back(void);
