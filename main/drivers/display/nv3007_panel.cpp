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

#include "drivers/display/nv3007_panel.h"

#include <cstring>

#include "conf/board.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ThetaGP::Drivers::Display {

void Nv3007Panel::init() {
  gpio_config_t outputs = {};
  outputs.pin_bit_mask = (1ULL << PIN_LCD_CS) | (1ULL << PIN_LCD_DC) |
                         (1ULL << PIN_LCD_RST) | (1ULL << PIN_LCD_BLK);
  outputs.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&outputs));
  gpio_set_level(PIN_LCD_CS, 1);
  gpio_set_level(PIN_LCD_BLK, 0);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_LCD_MOSI;
  bus.miso_io_num = -1;
  bus.sclk_io_num = PIN_LCD_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = LCD_V_RES * 20 * 2;
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t panel = {};
  panel.clock_speed_hz = LCD_SPI_HZ;
  panel.mode = 0;
  // The chip select is driven by hand, so a command and its parameters share
  // one window.
  panel.spics_io_num = -1;
  panel.queue_size = 2;
  ESP_ERROR_CHECK(spi_bus_add_device(LCD_SPI_HOST, &panel, &_spi));

  reset();
}

void Nv3007Panel::backlight(bool on) {
  gpio_set_level(PIN_LCD_BLK, on ? 1 : 0);
  printf("panel: backlight %s\n", on ? "on" : "off");
}

void Nv3007Panel::reset() {
  gpio_set_level(PIN_LCD_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(PIN_LCD_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(PIN_LCD_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(120));
}

void Nv3007Panel::sendCommand(lv_display_t *disp, const uint8_t *cmd,
                              size_t cmdSize, const uint8_t *param,
                              size_t paramSize) {
  (void)disp;
  Nv3007Panel &panel = getInstance();

  gpio_set_level(PIN_LCD_CS, 0);
  gpio_set_level(PIN_LCD_DC, 0);

  spi_transaction_t command = {};
  command.length = cmdSize * 8;
  command.tx_buffer = cmd;
  spi_device_polling_transmit(panel._spi, &command);

  if (param != nullptr && paramSize != 0) {
    gpio_set_level(PIN_LCD_DC, 1);
    spi_transaction_t body = {};
    body.length = paramSize * 8;
    body.tx_buffer = param;
    spi_device_polling_transmit(panel._spi, &body);
  }
  gpio_set_level(PIN_LCD_CS, 1);
}

void Nv3007Panel::sendColour(lv_display_t *disp, const uint8_t *cmd,
                             size_t cmdSize, uint8_t *param, size_t paramSize) {
  Nv3007Panel &panel = getInstance();

  if (param != nullptr && paramSize != 0) {
    gpio_set_level(PIN_LCD_CS, 0);
    gpio_set_level(PIN_LCD_DC, 0);

    spi_transaction_t command = {};
    command.length = cmdSize * 8;
    command.tx_buffer = cmd;
    spi_device_polling_transmit(panel._spi, &command);

    gpio_set_level(PIN_LCD_DC, 1);
    swapRgb565(param, paramSize);

    spi_transaction_t body = {};
    body.length = paramSize * 8;
    body.tx_buffer = param;
    spi_device_polling_transmit(panel._spi, &body);
    gpio_set_level(PIN_LCD_CS, 1);
  }
  lv_display_flush_ready(disp);
}

void Nv3007Panel::swapRgb565(uint8_t *bytes, size_t length) {
  for (size_t i = 0; i + 1 < length; i += 2) {
    const uint8_t first = bytes[i];
    bytes[i] = bytes[i + 1];
    bytes[i + 1] = first;
  }
}

} // namespace ThetaGP::Drivers::Display
