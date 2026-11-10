idf_component_register(
    SRCS "main.c" "menu.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_timer esp_driver_spi esp_driver_gpio)
