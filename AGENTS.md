# AGENTS.md

ESP32-S3 firmware (ESP-IDF) for the ThetaGP panel. Two independent builds share
`main/ui/menu.cpp`: the firmware, and a host simulator used for fast visual
checks without hardware.

## Build and run

Firmware (requires ESP-IDF `6.1.0` per `dependencies.lock`, with `IDF_PATH` set):

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
  `setup` inits `Nv3007Panel` → `LvglDisplay` → `Ui::Menu::init()` and seeds the
  model; `run` is an `lv_timer_handler()` loop.
- `main/conf/board.h` holds the panel pins and sizes. Native panel is 156x428,
  mounted sideways; LVGL rotates 270° so the UI surface is 428x156.
- `main/ui/menu.h` `Model` is the seam for the `ThetaGP.PB` protocol, which is
  **not wired in yet**. Protocol code should call the `Menu::set*` / navigation
  functions rather than touch widgets.
- `simulate/` is a separate CMake project that compiles the real
  `../main/ui/menu.cpp`. ESP-IDF calls are satisfied by stubs in
  `simulate/include/driver/`; `gpio_get_level` always returns idle, so the board
  button does nothing there — keyboard drives navigation.
- The simulator draws straight into SDL and never compiles
  `nv3007_panel.cpp` / `lvgl_display.cpp`; it validates the UI, not the SPI
  panel driver (including the RGB565 byte swap).

## LVGL and configuration gotchas

- LVGL is **not vendored**. `cmake/lvgl.cmake` fetches it at configure time into
  `lib/lvgl` (gitignored), pinned to commit `85aa60d…` (v9.5.0); a source tree
  already there (e.g. an unpacked release archive) is used as is, without git
  and without network. Do not edit `lib/lvgl`; change `THETAGP_LVGL_TAG` to
  move the pin.
- Two different LVGL configs:
  - Firmware: Kconfig via `sdkconfig.defaults`, plus `LV_USE_NV3007=1` added as a
    compile definition in the root `CMakeLists.txt` (LVGL 9.5.0 has no Kconfig
    switch for NV3007). Requires `CONFIG_LV_USE_GENERIC_MIPI=y`.
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
