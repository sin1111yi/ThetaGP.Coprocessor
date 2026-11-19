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

#include "drivers/display/lvgl_display.h"

#include <cstdio>

#include "conf/board.h"
#include "drivers/display/nv3007_lcd.h"

#include "esp_lv_adapter.h"

namespace ThetaGP::Drivers::Display {

void LvglDisplay::init() {
  Nv3007Lcd::getInstance().init();
  if (Nv3007Lcd::getInstance().panel() == nullptr) {
    return;
  }

  esp_lv_adapter_config_t config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
  // The widgets are driven on this core; the link reads its bytes on the other
  // one, and the frame queue is the only thing between them.
  config.task_core_id = 0;
  if (esp_lv_adapter_init(&config) != ESP_OK) {
    printf("nv3007: no adapter\n");
    return;
  }

  // MADCTL rotates the panel to 428x156; the adapter sends landscape pixels.
  esp_lv_adapter_display_config_t display =
      ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
          Nv3007Lcd::getInstance().panel(), Nv3007Lcd::getInstance().io(),
          LCD_V_RES, LCD_H_RES, ESP_LV_ADAPTER_ROTATE_0);
  _display = esp_lv_adapter_register_display(&display);
  if (_display == nullptr) {
    printf("nv3007: no display\n");
    return;
  }

  if (esp_lv_adapter_start() != ESP_OK) {
    printf("nv3007: adapter did not start\n");
    _display = nullptr;
    return;
  }
  printf("nv3007: %dx%d ready\n", LCD_V_RES, LCD_H_RES);
}

} // namespace ThetaGP::Drivers::Display
