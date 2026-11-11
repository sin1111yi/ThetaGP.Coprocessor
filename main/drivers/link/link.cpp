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

// The three steps one frame is read in.
constexpr uint8_t kPrefix = 0;
constexpr uint8_t kPayload = 1;
constexpr uint8_t kChecksum = 2;

uint16_t payloadSum(const uint8_t *payload, uint16_t length) {
  uint16_t sum = 0;
  for (uint16_t i = 0; i < length; ++i) {
    sum = static_cast<uint16_t>(sum + payload[i]);
  }
  return sum;
}

uint16_t encode(const uint8_t *payload, uint16_t length, uint8_t *out,
                uint16_t capacity) {
  const uint8_t prefix = length < 0x80u ? 1 : 2;
  const uint16_t total =
      static_cast<uint16_t>(prefix + length + sizeof(uint16_t));
  if (length > Link::PAYLOAD_MAX || total > capacity) {
    return 0;
  }

  uint16_t used = 0;
  if (prefix == 1) {
    out[used++] = static_cast<uint8_t>(length);
  } else {
    out[used++] = static_cast<uint8_t>((length & 0x7Fu) | 0x80u);
    out[used++] = static_cast<uint8_t>((length >> 7) & 0x7Fu);
  }
  for (uint16_t i = 0; i < length; ++i) {
    out[used++] = payload[i];
  }
  const uint16_t sum = payloadSum(payload, length);
  out[used++] = static_cast<uint8_t>(sum & 0xFFu);
  out[used++] = static_cast<uint8_t>((sum >> 8) & 0xFFu);
  return used;
}

// A payload that proves the two directions of the framing agree, so a link that
// never answers is a wire and not a codec.
void selfCheck() {
  const uint8_t sample[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x7F, 0x80};
  uint8_t framed[Link::FRAME_MAX] = {};
  const uint16_t size = encode(sample, sizeof sample, framed, sizeof framed);
  if (size == 0) {
    printf("link: codec broken (encode)\n");
    return;
  }

  uint16_t sum = 0;
  for (uint16_t i = 0; i < sizeof sample; ++i) {
    sum = static_cast<uint16_t>(sum + sample[i]);
  }
  const bool ok = framed[0] == sizeof sample &&
                  framed[size - 1] == (sum >> 8) &&
                  framed[size - 2] == (sum & 0xFF);
  printf("link: codec %s\n", ok ? "ok" : "broken");
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

  selfCheck();
  parserReset();

  // The pins looped inside the peripheral: the frame this end writes has to
  // come back byte for byte, which separates the wire from the code.
  uart_set_loop_back(kPort, true);
  uart_flush_input(kPort);
  const uint8_t probe[] = {'L', 'O', 'O', 'P'};
  uint8_t framed[FRAME_MAX] = {};
  const uint16_t size = encode(probe, sizeof probe, framed, sizeof framed);
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
    if (_refused != 0 && _refused != reported) {
      reported = _refused;
      printf("link: %u frames refused, %u B seen\n", (unsigned)_refused,
             (unsigned)_bytesRead);
    }

    const int read = uart_read_bytes(kPort, chunk, sizeof chunk,
                                     pdMS_TO_TICKS(kIdleTimeoutMs));
    _bytesRead = _bytesRead + (uint32_t)(read > 0 ? read : 0);
    for (int i = 0; i < read; ++i) {
      if (parserFeed(chunk[i])) {
        deliver(_payload, _completed);
      }
    }
  }
}

void Link::parserReset() {
  _length = 0;
  _filled = 0;
  _sum = 0;
  _prefixBytes = 0;
  _checksumLow = 0;
  _checksumBytes = 0;
  _state = kPrefix;
}

bool Link::parserFeed(uint8_t byte) {
  switch (_state) {
  case kPrefix:
    _length = _prefixBytes == 0
                  ? (uint16_t)(byte & 0x7Fu)
                  : (uint16_t)(_length | ((uint16_t)(byte & 0x7Fu) << 7));
    if (_prefixBytes == 0 && (byte & 0x80u) != 0) {
      // A length this small is one byte: anything else continues it.
      _prefixBytes = 1;
      return false;
    }
    if (_length > PAYLOAD_MAX) {
      // A length no frame could carry: give up on it and let the next byte
      // start again.
      _refused = _refused + 1;
      parserReset();
      return false;
    }
    _filled = 0;
    _sum = 0;
    _checksumBytes = 0;
    _state = _length == 0 ? kChecksum : kPayload;
    return false;

  case kPayload:
    _payload[_filled++] = byte;
    _sum = static_cast<uint16_t>(_sum + byte);
    if (_filled == _length) {
      _state = kChecksum;
      _checksumBytes = 0;
    }
    return false;

  case kChecksum:
    if (_checksumBytes == 0) {
      _checksumLow = byte;
      _checksumBytes = 1;
      return false;
    }
    if (_sum != (uint16_t)(_checksumLow | ((uint16_t)byte << 8))) {
      _refused = _refused + 1;
      parserReset();
      return false;
    }
    if (_length == 0) {
      // An idle or floating line reads as zero bytes, and three of them look
      // like an empty frame. Nothing carries a payload of none.
      _refused = _refused + 1;
      parserReset();
      return false;
    }
    _received = _received + 1;
    // The frame's length is the parser's until the reset below clears it.
    _completed = _length;
    parserReset();
    return true;

  default:
    parserReset();
    return false;
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
  const uint16_t size = encode(payload, length, framed, sizeof framed);
  if (size == 0) {
    return false;
  }
  return uart_write_bytes(kPort, framed, size) == size;
}

} // namespace ThetaGP::Drivers::Link
