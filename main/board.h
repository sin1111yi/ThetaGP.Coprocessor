#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

// The panel the coprocessor drives.
#define LCD_H_RES 156
#define LCD_V_RES 428

#define LCD_SPI_HOST SPI2_HOST
#define LCD_SPI_HZ (60 * 1000 * 1000)

#define PIN_LCD_BLK GPIO_NUM_13
#define PIN_LCD_CS GPIO_NUM_10
#define PIN_LCD_DC GPIO_NUM_9
#define PIN_LCD_RST GPIO_NUM_14
#define PIN_LCD_MOSI GPIO_NUM_11
#define PIN_LCD_SCK GPIO_NUM_12

// The board's BOOT button, used to walk the menu until the link is up.
#define PIN_BUTTON GPIO_NUM_0
