# AGENTS.md

ESP32-S3 firmware (ESP-IDF) for the ThetaGP panel. Two independent builds share
`main/ui/menu.cpp`: the firmware, and a host simulator used for fast visual
checks without hardware.

## Build and run

Firmware (requires ESP-IDF `6.1.0`, with `IDF_PATH` set; the codec generator
also needs `protoc` and python-protobuf installed for the system Python;
`cmake/link.cmake` prefers `/usr/bin/python3` over the IDF venv and stops the
configure with an error if either is missing):

- `idf.py build` — uses `sdkconfig.defaults`, output in `build/`
- Presets (Ninja): `cmake --preset esp32s3` then `cmake --build --preset esp32s3`
  (tree in `build/esp32s3`); `esp32s3-release` for Release
- Flash/monitor: `idf.py -p <port> flash monitor`; add `-B build/esp32s3` if
  flashing a preset tree

Simulator (no hardware; use this to verify UI changes):

- `python simulate/launch.py` — configures, builds, and runs
- `python simulate/launch.py --clean` to drop `simulate/build`
- Args after `--` are forwarded to the binary: `--scale F`, `--dump file.ppm`
  (headless: renders one frame to a PPM instead of opening a window)
- Keys: up/left previous, down/right next, enter enter, backspace back; close
  the window to quit
- Needs CMake and a C/C++ compiler; SDL2 is taken from `lib/sdl2` when it is
  checked out there, then from the system (CMake package, pkg-config, or
  `SDL2_DIR`), and fetched into `lib/sdl2` as a last resort

## Architecture

- Entry: `main/main.cpp` `app_main` → `Coprocessor::setup()` then `run()`.
  `setup` inits `LvglDisplay` (which brings up `Nv3007Lcd` and starts
  `esp_lvgl_adapter`) → `Ui::Menu::init()` and seeds the model. The adapter
  owns the LVGL task, tick and lock; `run` is the protocol loop, paced by a
  10 ms `esp_timer`, and touches widgets only under `esp_lv_adapter_lock`.
- `main/drivers/display/nv3007_lcd.cpp` is a hand-written `esp_lcd` panel for
  the NV3007: its power-up sequence plus window-and-write. The adapter already
  byte-swaps RGB565 and passes exclusive end coordinates to `draw_bitmap`; do
  not swap again in the driver.
- `main/conf/board.h` holds the panel pins and sizes. Native panel is 156x428,
  mounted sideways; MADCTL (`0x36 = 0x60`, last step of the init sequence)
  turns it so the UI surface is 428x156. The adapter does not rotate SPI
  panels, so do not use `lv_display_set_rotation`.
- `main/ui/menu.h` `Model` is the seam for the `ThetaGP.PB` protocol, which is
  **not wired in yet**. Protocol code should call the `Menu::set*` / navigation
  functions rather than touch widgets.
- `main/ui/spring.h` is the one place a damped ("springy") animation comes
  from. A page calls `Spring::run(key, tuning, from, to, apply, done)`; the
  position at each frame is `1 - e^(-decay t)(cos(omega t) + decay/omega
  sin(omega t))` and nothing else. Tunings (`Spring::CARDS`, `Spring::COLUMN`)
  live in that header; do not write another easing curve in a page.
- `simulate/` is a separate CMake project that compiles the real
  `../main/ui/menu.cpp`. ESP-IDF calls are satisfied by stubs in
  `simulate/include/driver/`; `gpio_get_level` always returns idle, so the board
  button does nothing there — keyboard drives navigation.
- The simulator draws straight into SDL and never compiles
  `nv3007_lcd.cpp` / `lvgl_display.cpp`; it validates the UI, not the SPI
  panel driver or the adapter (including the RGB565 byte swap).

## Dependencies

- Nothing is vendored. `lib/CMakeLists.txt` is the one place that pins and
  fetches every support library, at configure time, into gitignored
  directories under `lib/`; both builds `include()` it and call
  `thetagp_require(<name>...)`:
  - `lvgl` → `lib/lvgl`, git commit `85aa60d…` (v9.5.0)
  - `nanopb` → `lib/nanopb`, git commit `93550bb…` (0.4.9.3)
  - `sdl2` → `lib/sdl2`, git commit `5d24957…` (2.32.10), simulator only
  - `esp_lvgl_adapter` 0.7.8 → `lib/espressif__esp_lvgl_adapter` and
    `esp_lcd_touch` 1.2.1 → `lib/espressif__esp_lcd_touch`, registry archives
    checked against a SHA-256
- The ESP-IDF component manager is **off** (`IDF_COMPONENT_MANAGER=0`, set in
  the root `CMakeLists.txt`): there is no `idf_component.yml`,
  `dependencies.lock` or `managed_components/`. A new ESP component is a new
  pin in `lib/CMakeLists.txt`, its dependencies included, plus an entry in
  `EXTRA_COMPONENT_DIRS`.
- To move a pin, change its `THETAGP_*_TAG` / `_VERSION` + `_SHA256`; the next
  configure moves the checkout. A tree already in place without `.git` (or,
  for archives, without `.thetagp-pin`) is used as is, with no network. Do not
  edit the fetched trees.
- `lib/ThetaGP.PB` is the exception: a git submodule, pinned by the repository.

## LVGL and configuration gotchas

- Two different LVGL configs:
  - Firmware: Kconfig via `sdkconfig.defaults`. LVGL's own NV3007 / generic
    MIPI drivers are not used; the panel is driven through `esp_lcd`.
  - Simulator: `simulate/lv_conf.h`.
  - Keep both in sync (color depth 16, Montserrat 12/14/20 fonts, widget set).
- `sdkconfig.defaults` only seeds a fresh `sdkconfig`; delete the generated
  `sdkconfig` if a defaults change does not take effect.

## Adding code

- `main/CMakeLists.txt` lists every source explicitly. New firmware `.cpp` files
  must be added to `SRCS` or they will not build.
- The simulator only compiles `simulate/main_sim.cpp`, `../main/ui/menu.cpp`
  and `../main/ui/logo.c`. Shared code pulled in by `menu.cpp` must also be
  added to `simulate/CMakeLists.txt`, with stubs under `simulate/include/` for
  any ESP-IDF headers.

## Conventions

- Every source file begins with the GPLv3 comment header; new files should too.
- C++17, 2-space indent, ~80 columns, namespaces under `ThetaGP::`
  (`Drivers::Display`, `Ui`, `Ui::Theme`).
- No tests, linter, formatter config, or CI exist. Verification means building
  the simulator and, when possible, the firmware.
- Commit history follows Conventional Commits (`feat:`, `build:`, `refactor:`,
  `docs:`, `chore:`, with optional scopes).
