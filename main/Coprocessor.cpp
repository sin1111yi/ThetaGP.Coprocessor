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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

#include "drivers/display/lvgl_display.h"
#include "drivers/display/nv3007_panel.h"
#include "ui/menu.h"

namespace ThetaGP {

void Coprocessor::setup() {
  Drivers::Display::Nv3007Panel::getInstance().init();
  Drivers::Display::LvglDisplay::getInstance().init();
  if (Drivers::Display::LvglDisplay::getInstance().handle() == nullptr) {
    return;
  }

  Ui::Menu::init();
  // The values the panel was built against, until the protocol fills them.
  Ui::Menu::setLink(false);
  Ui::Menu::setProfile(1, 2, "Main");
  Ui::Menu::setReportHz(8000);
}

void Coprocessor::run() {
  if (Drivers::Display::LvglDisplay::getInstance().handle() == nullptr) {
    return;
  }

  for (;;) {
    uint32_t wait = lv_timer_handler();
    if (wait < 5) {
      wait = 5;
    }
    if (wait > 20) {
      wait = 20;
    }
    vTaskDelay(pdMS_TO_TICKS(wait));
  }
}

} // namespace ThetaGP
