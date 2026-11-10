#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "board.h"
#include "menu.h"

static spi_device_handle_t s_spi;
static lv_display_t *s_display;

static uint32_t tick_ms(void) {
  return (uint32_t)(esp_timer_get_time() / 1000);
}

static void panel_reset(void) {
  gpio_set_level(PIN_LCD_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(PIN_LCD_RST, 0);
  vTaskDelay(pdMS_TO_TICKS(20));
  gpio_set_level(PIN_LCD_RST, 1);
  vTaskDelay(pdMS_TO_TICKS(120));
}

// A command and its parameters inside one chip-select window, as the panel
// expects them.
static void send_cmd(lv_display_t *disp, const uint8_t *cmd, size_t cmd_size,
                     const uint8_t *param, size_t param_size) {
  (void)disp;
  gpio_set_level(PIN_LCD_CS, 0);
  gpio_set_level(PIN_LCD_DC, 0);

  spi_transaction_t command = {0};
  command.length = cmd_size * 8;
  command.tx_buffer = cmd;
  spi_device_polling_transmit(s_spi, &command);

  if (param != NULL && param_size != 0) {
    gpio_set_level(PIN_LCD_DC, 1);
    spi_transaction_t body = {0};
    body.length = param_size * 8;
    body.tx_buffer = param;
    spi_device_polling_transmit(s_spi, &body);
  }
  gpio_set_level(PIN_LCD_CS, 1);
}

// The panel takes RGB565 most significant byte first; LVGL's buffer is the
// other way round.
static void swap_rgb565(uint8_t *bytes, size_t len) {
  for (size_t i = 0; i + 1 < len; i += 2) {
    const uint8_t first = bytes[i];
    bytes[i] = bytes[i + 1];
    bytes[i + 1] = first;
  }
}

static void send_colour(lv_display_t *disp, const uint8_t *cmd,
                        size_t cmd_size, uint8_t *param, size_t param_size) {
  (void)disp;
  if (param != NULL && param_size != 0) {
    gpio_set_level(PIN_LCD_CS, 0);
    gpio_set_level(PIN_LCD_DC, 0);
    spi_transaction_t command = {0};
    command.length = cmd_size * 8;
    command.tx_buffer = cmd;
    spi_device_polling_transmit(s_spi, &command);

    gpio_set_level(PIN_LCD_DC, 1);
    swap_rgb565(param, param_size);

    spi_transaction_t body = {0};
    body.length = param_size * 8;
    body.tx_buffer = param;
    spi_device_polling_transmit(s_spi, &body);
    gpio_set_level(PIN_LCD_CS, 1);
  }
  lv_display_flush_ready(s_display);
}

static void display_init(void) {
  gpio_config_t outputs = {
      .pin_bit_mask = (1ULL << PIN_LCD_CS) | (1ULL << PIN_LCD_DC) |
                      (1ULL << PIN_LCD_RST) | (1ULL << PIN_LCD_BLK),
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&outputs));
  gpio_set_level(PIN_LCD_CS, 1);
  gpio_set_level(PIN_LCD_BLK, 1);

  spi_bus_config_t bus = {0};
  bus.mosi_io_num = PIN_LCD_MOSI;
  bus.miso_io_num = -1;
  bus.sclk_io_num = PIN_LCD_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = LCD_V_RES * 20 * 2;
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t panel = {0};
  panel.clock_speed_hz = LCD_SPI_HZ;
  panel.mode = 0;
  panel.spics_io_num = -1;
  panel.queue_size = 2;
  ESP_ERROR_CHECK(spi_bus_add_device(LCD_SPI_HOST, &panel, &s_spi));

  panel_reset();

  lv_init();
  lv_tick_set_cb(tick_ms);

  // The panel is 156x428 native and mounted across the board; the drawing
  // surface is the quarter turn that puts it upright.
  s_display = lv_nv3007_create(LCD_H_RES, LCD_V_RES, LV_LCD_FLAG_NONE, send_cmd,
                               send_colour);
  if (s_display == NULL) {
    printf("nv3007: no display\n");
    return;
  }
  lv_display_set_rotation(s_display, LV_DISPLAY_ROTATION_270);

  const size_t pixels = LCD_V_RES * 20;
  uint8_t *first =
      heap_caps_malloc(pixels * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  uint8_t *second =
      heap_caps_malloc(pixels * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (first == NULL || second == NULL) {
    printf("nv3007: no draw buffer\n");
    return;
  }
  lv_display_set_buffers(s_display, first, second, pixels * 2,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  printf("nv3007: %dx%d ready\n", LCD_H_RES, LCD_V_RES);
}

void app_main(void) {
  display_init();
  if (s_display == NULL) {
    return;
  }

  menu_init();
  menu_set_link(false);
  menu_set_profile(1, 2, "Main");
  menu_set_report_hz(8000);

  for (;;) {
    uint32_t wait = lv_timer_handler();
    if (wait < 5) {
      wait = 5;
    }
    vTaskDelay(pdMS_TO_TICKS(wait > 20 ? 20 : wait));
  }
}
