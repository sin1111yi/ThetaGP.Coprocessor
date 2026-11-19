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

#include "drivers/link/link.h"

#include <cstdio>
#include <cstring>

#include "conf/board.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "utils/log/log.h"

namespace ThetaGP::Drivers::Link {

namespace {

// The coprocessor's own UART: UART0 is the console the USB bridge carries.
constexpr uart_port_t kPort = UART_NUM_1;
constexpr int kRxBuffer = 2048;
constexpr uint32_t kIdleTimeoutMs = 20;
constexpr uint8_t kReadChunk = 64;

// The core the bytes are read on, away from the one the widgets are driven on:
// a frame arriving while the panel is flushed is read at once, and the flush is
// not held up by the read. The queue between the two is the frame layer's.
constexpr BaseType_t kCore = 1;
constexpr UBaseType_t kPriority = 6;
constexpr uint32_t kStack = 4096;

uint32_t nowMs(void) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

} // namespace

void Link::init() {
  uart_config_t config = {};
  config.baud_rate = LINK_BAUD;
  config.data_bits = UART_DATA_8_BITS;
  config.parity = UART_PARITY_DISABLE;
  config.stop_bits = UART_STOP_BITS_1;
  config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  config.source_clk = UART_SCLK_DEFAULT;

  ESP_ERROR_CHECK(uart_driver_install(kPort, kRxBuffer, 0, 0, nullptr, 0));
  ESP_ERROR_CHECK(uart_param_config(kPort, &config));
  ESP_ERROR_CHECK(uart_set_pin(kPort, PIN_LINK_TX, PIN_LINK_RX,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

  gpio_config_t ready = {};
  ready.pin_bit_mask = 1ULL << PIN_LINK_READY;
  ready.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&ready));
  gpio_set_level(PIN_LINK_READY, 0);

  LOG_INFO("link: codec %s", Wire::Frame::selfCheck() ? "ok" : "broken");

  // The pins looped inside the peripheral: the frame this end writes has to
  // come back byte for byte, which separates the wire from the code.
  uart_set_loop_back(kPort, true);
  uart_flush_input(kPort);
  const uint8_t probe[] = {'L', 'O', 'O', 'P'};
  uint8_t framed[FRAME_MAX] = {};
  const uint16_t size =
      Wire::Frame::encode(probe, sizeof probe, framed, sizeof framed);
  uint8_t echo[FRAME_MAX] = {};
  uart_write_bytes(kPort, framed, size);
  const int heard = uart_read_bytes(kPort, echo, size, pdMS_TO_TICKS(100));
  uart_set_loop_back(kPort, false);
  uart_flush_input(kPort);
  LOG_INFO("link: loopback %s (%d/%d B)",
           (heard == (int)size && memcmp(echo, framed, size) == 0) ? "ok"
                                                                  : "failed",
           heard, (int)size);

  xTaskCreatePinnedToCore(taskEntry, "link", kStack, this, kPriority, nullptr,
                          kCore);
  LOG_INFO("link: uart1 %d baud, tx %d rx %d", LINK_BAUD, PIN_LINK_TX,
           PIN_LINK_RX);
}

void Link::markReady() {
  if (_readyLine) {
    return;
  }
  gpio_set_level(PIN_LINK_READY, 1);
  _readyLine = true;
  LOG_INFO("link: ready");
}

void Link::taskEntry(void *argument) { static_cast<Link *>(argument)->run(); }

void Link::run() {
  uint8_t chunk[kReadChunk];
  uint32_t reported = 0;

  for (;;) {
    // A half frame whose bytes stopped arriving is dropped here, on the task
    // that read them.
    (void)_frame.tick(nowMs());

    // A line that carried nothing but noise says so once, when it first does.
    if (_frame.droppedFrames() != 0 && _frame.droppedFrames() != reported) {
      reported = _frame.droppedFrames();
      LOG_WARN("link: %u frames refused, %u B seen", (unsigned)reported,
               (unsigned)_bytesRead);
    }

    const int read = uart_read_bytes(kPort, chunk, sizeof chunk,
                                     pdMS_TO_TICKS(kIdleTimeoutMs));
    if (read > 0) {
      _bytesRead = _bytesRead + (uint32_t)read;
      _frame.feed(chunk, (uint16_t)read, nowMs());
    }
  }
}

bool Link::take(Wire::Frame::Payload &out) {
  if (!_frame.take(out)) {
    return false;
  }
  _lastFrameMs = nowMs();
  _taken = _taken + 1;
  return true;
}

uint32_t Link::refused(void) const { return _frame.droppedFrames(); }

bool Link::alive(void) const {
  const uint32_t now = nowMs();
  const uint32_t last = _lastFrameMs;
  return last != 0 && static_cast<uint32_t>(now - last) < 1500;
}

bool Link::send(const uint8_t *payload, uint16_t length) {
  uint8_t framed[FRAME_MAX] = {};
  const uint16_t size = Wire::Frame::encode(payload, length, framed,
                                            sizeof framed);
  if (size == 0) {
    return false;
  }
  return uart_write_bytes(kPort, framed, size) == size;
}

} // namespace ThetaGP::Drivers::Link
