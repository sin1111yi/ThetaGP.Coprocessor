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

#include "app/Coprocessor.h"

#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

#include "drivers/display/lvgl_display.h"

#include "esp_lv_adapter.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "conf/board.h"
#include "drivers/display/nv3007_lcd.h"
#include "drivers/input/button.h"
#include "drivers/link/link.h"
#include "protocol/panel_protocol.h"
#include "ui/menu.h"

namespace ThetaGP {

namespace {

// The protocol speaks over the link and reports the panel; this file is what
// knows the board, so the wiring is here and the message layer names no driver.

bool sendOnLink(const uint8_t *payload, uint16_t length, void *context) {
  (void)context;
  return Drivers::Link::Link::getInstance().send(payload, length);
}

bool heardOnLink(void *context) {
  (void)context;
  return Drivers::Link::Link::getInstance().heard();
}

uint32_t refusedOnLink(void *context) {
  (void)context;
  return Drivers::Link::Link::getInstance().refused();
}

// The surface the menu draws on: the panel is mounted on its side, so the
// wider number is the width.
uint16_t panelWidth(void *context) {
  (void)context;
  return LCD_V_RES;
}

uint16_t panelHeight(void *context) {
  (void)context;
  return LCD_H_RES;
}

bool backlightOn(void *context) {
  (void)context;
  return Drivers::Display::Nv3007Lcd::getInstance().backlightOn();
}

// What the link hands a payload to.
void onFrame(const uint8_t *payload, uint16_t length, void *context) {
  static_cast<Protocol::PanelProtocol *>(context)->deliver(payload, length);
}

const Protocol::Transport kTransport = {sendOnLink, heardOnLink, refusedOnLink,
                                        nullptr};
const Protocol::Panel kPanel = {panelWidth, panelHeight, backlightOn, nullptr};

} // namespace

void Coprocessor::setup() {
  // The display brings the panel up and starts the LVGL task the adapter owns.
  Drivers::Display::LvglDisplay::getInstance().init();
  if (Drivers::Display::LvglDisplay::getInstance().handle() == nullptr) {
    // A panel that did not come up is not something to idle behind: the
    // reason is already on the console, and the board tries again.
    printf("coprocessor: no display, restarting\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
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

  // The protocol's own timer: a hardware one, so the cadence does not depend
  // on how coarse the kernel's tick is.
  static TaskHandle_t loop = xTaskGetCurrentTaskHandle();
  esp_timer_handle_t tick = nullptr;
  esp_timer_create_args_t args = {};
  args.callback = [](void *) { xTaskNotifyGive(loop); };
  args.name = "proto";
  args.dispatch_method = ESP_TIMER_TASK;
  args.skip_unhandled_events = true;
  // There is no second clock to fall back on: the loop below waits on this
  // timer alone, so a timer that cannot be had stops the board here.
  ESP_ERROR_CHECK(esp_timer_create(&args, &tick));
  ESP_ERROR_CHECK(esp_timer_start_periodic(tick, 10 * 1000));

  Drivers::Input::Button::getInstance().init();

  // The panel says whether the main board is answering. The link is read here,
  // on the task that owns the widgets, and only a change reaches the menu.
  bool linked = false;
  bool started = false;
  bool introSeen = false;
  uint32_t introAt = 0;

  for (;;) {
    // Nothing is asked of the main board until the wordmark has played out:
    // the bottom line is the menu's, not a screen the splash is still covering.
    // The home page then gets a second to itself before anything moves. The
    // wordmark is the LVGL task's to end, so it is asked under its lock.
    if (!started && !introSeen) {
      bool played = false;
      if (esp_lv_adapter_lock(-1) == ESP_OK) {
        played = Ui::Menu::introDone();
        esp_lv_adapter_unlock();
      }
      if (played) {
        introSeen = true;
        // The wait is the protocol's, not the screen's: a hardware timer keeps
        // it out of LVGL's own clock.
        introAt = (uint32_t)(esp_timer_get_time() / 1000);
      }
    }
    if (!started && introSeen &&
        (uint32_t)((uint32_t)(esp_timer_get_time() / 1000) - introAt) >= 1000U) {
      started = true;
      Drivers::Link::Link &link = Drivers::Link::Link::getInstance();
      link.init();
      // A frame the link assembles is the protocol's: the handler goes in
      // before the board is told the decoder is listening.
      link.setHandler(onFrame, &Protocol::PanelProtocol::getInstance());
      link.markReady();
      // The schema, on top of the bytes: the first hello names this panel, and
      // the beat it sends reads the menu, so it is sent under the same lock.
      if (esp_lv_adapter_lock(-1) == ESP_OK) {
        Protocol::PanelProtocol::getInstance().init(kTransport, kPanel);
        esp_lv_adapter_unlock();
      }
    }

    // The board button walks the menu until the link's own buttons arrive: a
    // short press moves the mark, a long one opens. The line is read freely;
    // the menu is touched only under the adapter's lock.
    const Drivers::Input::Press press =
        Drivers::Input::Button::getInstance().poll(lv_tick_get());
    if (press != Drivers::Input::Press::None &&
        esp_lv_adapter_lock(-1) == ESP_OK) {
      if (press == Drivers::Input::Press::Long) {
        Ui::Menu::enter();
      } else {
        Ui::Menu::next();
      }
      esp_lv_adapter_unlock();
    }

    if (started) {
      // The widgets belong to the adapter's task: what is applied here is
      // applied under its lock.
      if (esp_lv_adapter_lock(-1) == ESP_OK) {
        Protocol::PanelProtocol::getInstance().tick();
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
    // clock on a hardware timer, whose 10 ms does not drift with the tick.
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

} // namespace ThetaGP
