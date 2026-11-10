#pragma once

// The host build has no GPIO block: the pins exist only so that board.h and
// the menu's button timer compile unchanged. The simulated button is idle.
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  GPIO_NUM_0 = 0,
  GPIO_NUM_1,
  GPIO_NUM_2,
  GPIO_NUM_3,
  GPIO_NUM_4,
  GPIO_NUM_5,
  GPIO_NUM_6,
  GPIO_NUM_7,
  GPIO_NUM_8,
  GPIO_NUM_9,
  GPIO_NUM_10,
  GPIO_NUM_11,
  GPIO_NUM_12,
  GPIO_NUM_13,
  GPIO_NUM_14,
} gpio_num_t;

typedef enum {
  GPIO_MODE_DISABLE = 0,
  GPIO_MODE_INPUT,
  GPIO_MODE_OUTPUT,
} gpio_mode_t;

typedef enum {
  GPIO_PULLUP_DISABLE = 0,
  GPIO_PULLUP_ENABLE,
} gpio_pullup_t;

typedef enum {
  GPIO_PULLDOWN_DISABLE = 0,
  GPIO_PULLDOWN_ENABLE,
} gpio_pulldown_t;

typedef struct {
  uint64_t pin_bit_mask;
  gpio_mode_t mode;
  gpio_pullup_t pull_up_en;
  gpio_pulldown_t pull_down_en;
  int intr_type;
} gpio_config_t;

static inline int gpio_get_level(gpio_num_t pin) {
  (void)pin;
  return 1;
}

static inline int gpio_config(const gpio_config_t *config) {
  (void)config;
  return 0;
}
