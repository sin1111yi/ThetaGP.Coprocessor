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

// The board this firmware runs on: the panel it drives and the one line it has
// to a button. The display is mounted on its side, so the surface the menu
// draws on is the quarter turn of the native size.

#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

// The panel the coprocessor drives.
#define LCD_H_RES      156
#define LCD_V_RES      428

#define LCD_SPI_HOST   SPI2_HOST
#define LCD_SPI_HZ     (60 * 1000 * 1000)

#define PIN_LCD_BLK    GPIO_NUM_13
#define PIN_LCD_CS     GPIO_NUM_10
#define PIN_LCD_DC     GPIO_NUM_9
#define PIN_LCD_RST    GPIO_NUM_14
#define PIN_LCD_MOSI   GPIO_NUM_11
#define PIN_LCD_SCK    GPIO_NUM_12

// The board's BOOT button, used to walk the menu until the link is up.
#define PIN_BUTTON     GPIO_NUM_0

// The link to the main board: UART4 on its side, a plain UART here.
#define PIN_LINK_TX    GPIO_NUM_4 // to PC11 (UART4_RX)
#define PIN_LINK_RX    GPIO_NUM_5 // from PC10 (UART4_TX)
#define PIN_LINK_READY GPIO_NUM_6 // to PC12
#define LINK_BAUD      4000000
