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

namespace ThetaGP::Drivers::Link {

namespace {

// The coprocessor's own UART: UART0 is the console the USB bridge carries.
constexpr uart_port_t kPort = UART_NUM_1;
constexpr int kRxBuffer = 2048;
constexpr uint32_t kIdleTimeoutMs = 20;
constexpr uint8_t kReadChunk = 64;

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

  Frame::selfCheck();
  _parser.reset();

  // The pins looped inside the peripheral: the frame this end writes has to
  // come back byte for byte, which separates the wire from the code.
  uart_set_loop_back(kPort, true);
  uart_flush_input(kPort);
  const uint8_t probe[] = {'L', 'O', 'O', 'P'};
  uint8_t framed[FRAME_MAX] = {};
  const uint16_t size =
      Frame::encode(probe, sizeof probe, framed, sizeof framed);
  uint8_t echo[FRAME_MAX] = {};
  uart_write_bytes(kPort, framed, size);
  const int heard = uart_read_bytes(kPort, echo, size, pdMS_TO_TICKS(100));
  uart_set_loop_back(kPort, false);
  uart_flush_input(kPort);
  printf("link: loopback %s (%d/%d B)\n",
         (heard == (int)size && memcmp(echo, framed, size) == 0) ? "ok"
                                                                 : "failed",
         heard, (int)size);

  xTaskCreatePinnedToCore(taskEntry, "link", 4096, this, 6, nullptr, 1);
  printf("link: uart1 %d baud, tx %d rx %d\n", LINK_BAUD, PIN_LINK_TX,
         PIN_LINK_RX);
}

void Link::markReady() {
  if (_readyLine) {
    return;
  }
  gpio_set_level(PIN_LINK_READY, 1);
  _readyLine = true;
  printf("link: ready\n");
}

void Link::taskEntry(void *argument) { static_cast<Link *>(argument)->run(); }

void Link::run() {
  uint8_t chunk[kReadChunk];
  uint32_t reported = 0;

  for (;;) {
    // A line that carried nothing but noise says so once, when it first does.
    if (_parser.refused() != 0 && _parser.refused() != reported) {
      reported = _parser.refused();
      printf("link: %u frames refused, %u B seen\n", (unsigned)reported,
             (unsigned)_bytesRead);
    }

    const int read = uart_read_bytes(kPort, chunk, sizeof chunk,
                                     pdMS_TO_TICKS(kIdleTimeoutMs));
    _bytesRead = _bytesRead + (uint32_t)(read > 0 ? read : 0);
    for (int i = 0; i < read; ++i) {
      if (_parser.feed(chunk[i])) {
        deliver(_parser.payload(), _parser.length());
      }
    }
  }
}

bool Link::alive() const {
  const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
  const uint32_t last = _lastFrameMs;
  return last != 0 && static_cast<uint32_t>(now - last) < 1500;
}

void Link::deliver(const uint8_t *payload, uint16_t length) {
  _lastFrameMs = static_cast<uint32_t>(esp_timer_get_time() / 1000);
  if (_handler != nullptr) {
    _handler(payload, length, _context);
  }
}

bool Link::send(const uint8_t *payload, uint16_t length) {
  uint8_t framed[FRAME_MAX] = {};
  const uint16_t size = Frame::encode(payload, length, framed, sizeof framed);
  if (size == 0) {
    return false;
  }
  return uart_write_bytes(kPort, framed, size) == size;
}

} // namespace ThetaGP::Drivers::Link
