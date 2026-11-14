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

#include "Coprocessor.h"

#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

#include "drivers/display/lvgl_display.h"

#include "esp_lv_adapter.h"
#include "drivers/display/nv3007_lcd.h"
#include "drivers/link/link.h"
#include "drivers/link/panel_protocol.h"
#include "ui/menu.h"

namespace ThetaGP {

void Coprocessor::setup() {
  // The display brings the panel up and starts the LVGL task the adapter owns.
  Drivers::Display::LvglDisplay::getInstance().init();
  if (Drivers::Display::LvglDisplay::getInstance().handle() == nullptr) {
    return;
  }

  // A whole black screen goes to the glass before the backlight comes on, so
  // the panel never shows the buffer it powered up with.
  if (esp_lv_adapter_lock(-1) == ESP_OK) {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    esp_lv_adapter_unlock();
  }
  esp_lv_adapter_refresh_now(Drivers::Display::LvglDisplay::getInstance().handle());

  // A moment on black: the panel's own power-up settling is not something to
  // show, so the light comes on once the glass is already dark and held.
  vTaskDelay(pdMS_TO_TICKS(300));
  Drivers::Display::Nv3007Lcd::getInstance().backlight(true);

  if (esp_lv_adapter_lock(-1) == ESP_OK) {
    Ui::Menu::init();
    // The values the panel was built against, until the protocol fills them.
    Ui::Menu::setLink(false);
    Ui::Menu::setProfile(1, 2, "Main");
    Ui::Menu::setReportHz(8000);
    esp_lv_adapter_unlock();
  }
}

void Coprocessor::run() {
  if (Drivers::Display::LvglDisplay::getInstance().handle() == nullptr) {
    return;
  }

  // The panel says whether the main board is answering. The link is read here,
  // on the task that owns the widgets, and only a change reaches the menu.
  bool linked = false;
  bool started = false;
  bool introSeen = false;
  uint32_t introAt = 0;

  for (;;) {
    // Nothing is asked of the main board until the wordmark has played out:
    // the bottom line is the menu's, not a screen the splash is still covering.
    // The home page then gets a second to itself before anything moves.
    if (!started && Ui::Menu::introDone()) {
      if (!introSeen) {
        introSeen = true;
        introAt = lv_tick_get();
      }
    }
    if (!started && introSeen && (uint32_t)(lv_tick_get() - introAt) >= 1000U) {
      started = true;
      Drivers::Link::Link::getInstance().init();
      Drivers::Link::Link::getInstance().markReady();
      // The schema, on top of the bytes: the first hello names this panel.
      Drivers::Link::PanelProtocol::getInstance().init();
    }

    if (started) {
      // The widgets belong to the adapter's task: what is applied here is
      // applied under its lock.
      if (esp_lv_adapter_lock(-1) == ESP_OK) {
        Drivers::Link::PanelProtocol::getInstance().tick();
        // The menu says whether the board is answering, and changes once when
        // that answer starts or stops.
        const bool up = Drivers::Link::Link::getInstance().alive();
        if (up != linked) {
          linked = up;
          Ui::Menu::setLink(up);
        }
        esp_lv_adapter_unlock();
      }
    }

    // LVGL runs on the adapter's task now; this one keeps the protocol's own
    // clock, and leaves the screen alone between the calls above.
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

} // namespace ThetaGP
