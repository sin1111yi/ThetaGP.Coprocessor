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
#include "drivers/display/nv3007_init.h"
#include "utils/log/log.h"

#include "driver/gpio.h"
#include "esp_lcd_panel_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ThetaGP::Drivers::Display {

namespace {

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
  LOG_INFO("panel: backlight %s", on ? "on" : "off");
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
  LOG_INFO("nv3007: io %s (%d), %u Hz asked", esp_err_to_name(made), (int)made,
           (unsigned)LCD_SPI_HZ);
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
  for (size_t i = 0; i < Nv3007::INIT_COUNT; ++i) {
    const Nv3007::Step &step = Nv3007::INIT[i];
    if (step.delayMs != 0) {
      vTaskDelay(pdMS_TO_TICKS(step.delayMs));
      continue;
    }
    const esp_err_t sent =
        esp_lcd_panel_io_tx_param(_io, step.reg, step.data, step.count);
    if (sent != ESP_OK) {
      LOG_ERROR("nv3007: command %02x refused: %s", step.reg,
             esp_err_to_name(sent));
      _panel = nullptr;
      return;
    }
  }
}

} // namespace ThetaGP::Drivers::Display
