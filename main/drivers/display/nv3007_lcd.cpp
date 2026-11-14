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

#include "drivers/display/nv3007_lcd.h"

#include <cstdio>

#include "conf/board.h"

#include "driver/gpio.h"
#include "esp_lcd_panel_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ThetaGP::Drivers::Display {

namespace {

// One step of the panel's power-up: a command with its parameters, or a wait
// the panel asks for between them.
struct Step {
  uint8_t reg;
  uint8_t count;
  uint8_t data[16];
  uint16_t delayMs;
};

static const Step kInit[] = {
    {0x10, 0, {}, 0},
    {0x00, 0, {}, 10},
    {0x01, 0, {}, 0},
    {0x00, 0, {}, 200},
    {0x11, 0, {}, 0},
    {0x00, 0, {}, 300},
    {0x13, 0, {}, 0},
    {0x36, 1, {0x00}, 0},
    {0x3a, 1, {0x55}, 0},
    {0x29, 0, {}, 0},
    {0xff, 1, {0xa5}, 0},
    {0x9a, 1, {0x08}, 0},
    {0x9b, 1, {0x08}, 0},
    {0x9c, 1, {0xb0}, 0},
    {0x9d, 1, {0x16}, 0},
    {0x9e, 1, {0xc4}, 0},
    {0x8f, 2, {0x55, 0x04}, 0},
    {0x84, 1, {0x90}, 0},
    {0x83, 1, {0x7b}, 0},
    {0x85, 1, {0x33}, 0},
    {0x60, 1, {0x00}, 0},
    {0x70, 1, {0x00}, 0},
    {0x61, 1, {0x02}, 0},
    {0x71, 1, {0x02}, 0},
    {0x62, 1, {0x04}, 0},
    {0x72, 1, {0x04}, 0},
    {0x6c, 1, {0x29}, 0},
    {0x7c, 1, {0x29}, 0},
    {0x6d, 1, {0x31}, 0},
    {0x7d, 1, {0x31}, 0},
    {0x6e, 1, {0x0f}, 0},
    {0x7e, 1, {0x0f}, 0},
    {0x66, 1, {0x21}, 0},
    {0x76, 1, {0x21}, 0},
    {0x68, 1, {0x3a}, 0},
    {0x78, 1, {0x3a}, 0},
    {0x63, 1, {0x07}, 0},
    {0x73, 1, {0x07}, 0},
    {0x64, 1, {0x05}, 0},
    {0x74, 1, {0x05}, 0},
    {0x65, 1, {0x02}, 0},
    {0x75, 1, {0x02}, 0},
    {0x67, 1, {0x23}, 0},
    {0x77, 1, {0x23}, 0},
    {0x69, 1, {0x08}, 0},
    {0x79, 1, {0x08}, 0},
    {0x6a, 1, {0x13}, 0},
    {0x7a, 1, {0x13}, 0},
    {0x6b, 1, {0x13}, 0},
    {0x7b, 1, {0x13}, 0},
    {0x6f, 1, {0x00}, 0},
    {0x7f, 1, {0x00}, 0},
    {0x50, 1, {0x00}, 0},
    {0x52, 1, {0xd6}, 0},
    {0x53, 1, {0x08}, 0},
    {0x54, 1, {0x08}, 0},
    {0x55, 1, {0x1e}, 0},
    {0x56, 1, {0x1c}, 0},
    {0xa0, 3, {0x2b, 0x24, 0x00}, 0},
    {0xa1, 1, {0x87}, 0},
    {0xa2, 1, {0x86}, 0},
    {0xa5, 1, {0x00}, 0},
    {0xa6, 1, {0x00}, 0},
    {0xa7, 1, {0x00}, 0},
    {0xa8, 1, {0x36}, 0},
    {0xa9, 1, {0x7e}, 0},
    {0xaa, 1, {0x7e}, 0},
    {0xb9, 1, {0x85}, 0},
    {0xba, 1, {0x84}, 0},
    {0xbb, 1, {0x83}, 0},
    {0xbc, 1, {0x82}, 0},
    {0xbd, 1, {0x81}, 0},
    {0xbe, 1, {0x80}, 0},
    {0xbf, 1, {0x01}, 0},
    {0xc0, 1, {0x02}, 0},
    {0xc1, 1, {0x00}, 0},
    {0xc2, 1, {0x00}, 0},
    {0xc3, 1, {0x00}, 0},
    {0xc4, 1, {0x33}, 0},
    {0xc5, 1, {0x7e}, 0},
    {0xc6, 1, {0x7e}, 0},
    {0xc8, 2, {0x33, 0x33}, 0},
    {0xc9, 1, {0x68}, 0},
    {0xca, 1, {0x69}, 0},
    {0xcb, 1, {0x6a}, 0},
    {0xcc, 1, {0x6b}, 0},
    {0xcd, 2, {0x33, 0x33}, 0},
    {0xce, 1, {0x6c}, 0},
    {0xcf, 1, {0x6d}, 0},
    {0xd0, 1, {0x6e}, 0},
    {0xd1, 1, {0x6f}, 0},
    {0xab, 2, {0x03, 0x67}, 0},
    {0xac, 2, {0x03, 0x6b}, 0},
    {0xad, 2, {0x03, 0x68}, 0},
    {0xae, 2, {0x03, 0x6c}, 0},
    {0xb3, 1, {0x00}, 0},
    {0xb4, 1, {0x00}, 0},
    {0xb5, 1, {0x00}, 0},
    {0xb6, 1, {0x32}, 0},
    {0xb7, 1, {0x7e}, 0},
    {0xb8, 1, {0x7e}, 0},
    {0xe0, 1, {0x00}, 0},
    {0xe1, 2, {0x03, 0x0f}, 0},
    {0xe2, 1, {0x04}, 0},
    {0xe3, 1, {0x01}, 0},
    {0xe4, 1, {0x0e}, 0},
    {0xe5, 1, {0x01}, 0},
    {0xe6, 1, {0x19}, 0},
    {0xe7, 1, {0x10}, 0},
    {0xe8, 1, {0x10}, 0},
    {0xea, 1, {0x12}, 0},
    {0xeb, 1, {0xd0}, 0},
    {0xec, 1, {0x04}, 0},
    {0xed, 1, {0x07}, 0},
    {0xee, 1, {0x07}, 0},
    {0xef, 1, {0x09}, 0},
    {0xf0, 1, {0xd0}, 0},
    {0xf1, 1, {0x0e}, 0},
    {0xf9, 1, {0x17}, 0},
    {0xf2, 4, {0x2c, 0x1b, 0x0b, 0x20}, 0},
    {0xe9, 1, {0x29}, 0},
    {0xec, 1, {0x04}, 0},
    {0x35, 1, {0x00}, 0},
    {0x44, 2, {0x00, 0x10}, 0},
    {0x46, 1, {0x10}, 0},
    {0xff, 1, {0x00}, 0},
    {0x3a, 1, {0x05}, 0},
    {0x11, 0, {}, 0},
    {0x00, 0, {}, 220},
    {0x29, 0, {}, 0},
    {0x36, 1, {0x60}, 0},
};

// The adapter supplies packed RGB565 bytes in panel wire order, in a buffer
// it leaves alone until the transfer reports done: they go out as they are.
static esp_lcd_panel_io_handle_t s_io;

esp_err_t panelReset(esp_lcd_panel_t *panel) {
  (void)panel;
  gpio_set_level(PIN_LCD_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(PIN_LCD_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(PIN_LCD_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(120));
  return ESP_OK;
}

esp_err_t panelInit(esp_lcd_panel_t *panel) {
  (void)panel;
  return ESP_OK;
}

esp_err_t panelDraw(esp_lcd_panel_t *panel, int x1, int y1, int x2, int y2,
                    const void *data) {
  (void)panel;
  if (s_io == nullptr || data == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }

  // ESP LCD rectangles have exclusive end coordinates.
  if (x1 < 0 || y1 < 0 || x2 > LCD_V_RES || y2 > LCD_H_RES ||
      x2 <= x1 || y2 <= y1) {
    return ESP_ERR_INVALID_ARG;
  }
  const size_t bytes = (size_t)(x2 - x1) * (size_t)(y2 - y1) * 2;

  const uint8_t cols[4] = {(uint8_t)(x1 >> 8), (uint8_t)x1,
                           (uint8_t)((x2 - 1) >> 8), (uint8_t)(x2 - 1)};
  const uint8_t rows[4] = {(uint8_t)(y1 >> 8), (uint8_t)y1,
                           (uint8_t)((y2 - 1) >> 8), (uint8_t)(y2 - 1)};
  esp_err_t sent = esp_lcd_panel_io_tx_param(s_io, 0x2A, cols, sizeof cols);
  if (sent != ESP_OK) {
    return sent;
  }
  sent = esp_lcd_panel_io_tx_param(s_io, 0x2B, rows, sizeof rows);
  if (sent != ESP_OK) {
    return sent;
  }
  // One color transfer, however the bus splits it, produces one completion
  // notification to the adapter.
  return esp_lcd_panel_io_tx_color(s_io, 0x2C, data, bytes);
}

esp_err_t panelDel(esp_lcd_panel_t *panel) {
  (void)panel;
  return ESP_OK;
}

esp_err_t panelNoMirror(esp_lcd_panel_t *panel, bool x, bool y) {
  (void)panel;
  (void)x;
  (void)y;
  return ESP_OK;
}

esp_err_t panelNoSwap(esp_lcd_panel_t *panel, bool swap) {
  (void)panel;
  (void)swap;
  return ESP_OK;
}

esp_err_t panelNoGap(esp_lcd_panel_t *panel, int x, int y) {
  (void)panel;
  (void)x;
  (void)y;
  return ESP_OK;
}

esp_err_t panelNoInvert(esp_lcd_panel_t *panel, bool invert) {
  (void)panel;
  (void)invert;
  return ESP_OK;
}

esp_err_t panelNoOnOff(esp_lcd_panel_t *panel, bool on) {
  (void)panel;
  (void)on;
  return ESP_OK;
}

esp_err_t panelSleep(esp_lcd_panel_t *panel, bool sleep) {
  (void)panel;
  (void)sleep;
  return ESP_OK;
}

} // namespace

void Nv3007Lcd::backlight(bool on) {
  _backlightOn = on;
  gpio_set_level(PIN_LCD_BLK, on ? 1 : 0);
  printf("panel: backlight %s\n", on ? "on" : "off");
}

void Nv3007Lcd::init() {
  gpio_config_t outputs = {};
  outputs.pin_bit_mask = (1ULL << PIN_LCD_RST) | (1ULL << PIN_LCD_BLK);
  outputs.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&outputs));
  gpio_set_level(PIN_LCD_BLK, 0);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_LCD_MOSI;
  bus.miso_io_num = -1;
  bus.sclk_io_num = PIN_LCD_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = LCD_V_RES * 20 * 2;
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_spi_config_t io = {};
  io.cs_gpio_num = PIN_LCD_CS;
  io.dc_gpio_num = PIN_LCD_DC;
  io.spi_mode = 0;
  io.pclk_hz = LCD_SPI_HZ;
  io.trans_queue_depth = 10;
  io.lcd_cmd_bits = 8;
  io.lcd_param_bits = 8;
  esp_err_t made = esp_lcd_new_panel_io_spi(
      static_cast<esp_lcd_spi_bus_handle_t>(LCD_SPI_HOST), &io, &_io);
  // The bus divides its own clock down to the nearest rate at or under this.
  printf("nv3007: io %s (%d), %u Hz asked\n", esp_err_to_name(made),
         (int)made, (unsigned)LCD_SPI_HZ);
  if (made != ESP_OK) {
    return;
  }
  s_io = _io;

  auto *panel = new esp_lcd_panel_t();
  panel->reset = panelReset;
  panel->init = panelInit;
  panel->del = panelDel;
  panel->draw_bitmap = panelDraw;
  panel->draw_bitmap_2d = nullptr;
  panel->mirror = panelNoMirror;
  panel->swap_xy = panelNoSwap;
  panel->set_gap = panelNoGap;
  panel->invert_color = panelNoInvert;
  panel->disp_on_off = panelNoOnOff;
  panel->disp_sleep = panelSleep;
  _panel = panel;

  (void)panelReset(_panel);
  printf("nv3007: %u commands\n", (unsigned)(sizeof kInit / sizeof kInit[0]));
  for (const Step &step : kInit) {
    if (step.delayMs != 0) {
      vTaskDelay(pdMS_TO_TICKS(step.delayMs));
      continue;
    }
    const esp_err_t sent =
        esp_lcd_panel_io_tx_param(_io, step.reg, step.data, step.count);
    if (sent != ESP_OK) {
      printf("nv3007: command %02x refused: %s\n", step.reg,
             esp_err_to_name(sent));
      _panel = nullptr;
      return;
    }
  }
}

} // namespace ThetaGP::Drivers::Display
