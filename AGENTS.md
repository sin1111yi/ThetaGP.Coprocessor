# AGENTS.md

ESP32-S3 firmware (ESP-IDF) for the ThetaGP panel. Two independent builds share
the sources under `main/ui/`: the firmware, and a host simulator used for fast
visual checks without hardware.

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

- Layout, and the only way a layer may look: `app/` → `protocol/` → `ui/`,
  with `app/` → `drivers/`. `ui/` includes neither `protocol/` nor `drivers/`,
  and `protocol/` names no driver at all: the app hands it a `Transport`
  (send, heard, refused) and a `Panel` (width, height, backlight) at `init`.
  - `main/app/` — `Coprocessor.{h,cpp}`: boot order, the protocol loop, the
    board button turned into menu actions.
  - `main/wire/` — `frame.{h,cpp}` is the framing (a varint length, the
    payload, its sum) with a queue of whole frames and the reasons a frame is
    dropped; `dispatch.{h,cpp}` reads one payload and leaves what it asked for
    in an `Inbox` (the one place a new message kind is added). Neither names a
    message field of the board's, a driver, or a widget, and both build on a
    desk.
  - `main/protocol/` — `panel_protocol.{h,cpp}` is the session: hello, beat,
    shape, and applying an inbox on the task that owns the widgets.
    `transport.h` is the seam the app fills in.
  - `main/ui/` — `model.{h,cpp}` (what the protocol writes and the pages
    read), `menu.{h,cpp}` (page stack and navigation), `splash.{h,cpp}`,
    `theme.h`, `spring.{h,cpp}`, `pages/` (one file a page plus `pages.h`),
    `profile/` (the profile level: `profile_data`, `profile_names`,
    `profile_menu`, `profile_popup`, `profile`), `widgets/` (`column`),
    `assets/` (`logo`).
  - `main/drivers/` — `display/` (`lvgl_display`, `nv3007_lcd`,
    `nv3007_init`), `input/` (`button`), `link/` (`link`: the UART, the ready
    line, the task that reads it).
- Entry: `main/main.cpp` `app_main` → `app/Coprocessor.{h,cpp}`: `setup()`
  then `run()`. `setup` inits `LvglDisplay` (which brings up `Nv3007Lcd` and
  starts `esp_lvgl_adapter`) → `Ui::Menu::init()` and seeds the model. The
  adapter owns the LVGL task, tick and lock; `run` is the protocol loop, paced
  by a 10 ms `esp_timer`, and touches widgets only under `esp_lv_adapter_lock`.
- Both cores are used, and the split is by who owns what: the LVGL task is
  pinned to core 0 (`config.task_core_id`), the link task reads the UART on
  core 1, and the app loop runs on core 0 with the widgets
  (`ESP_MAIN_TASK_AFFINITY_CPU0`). The only thing between them is
  `Wire::Frame`'s slot ring, and three invariants hold it together:
  - one producer (`feed`/`tick` on the link task) and one consumer (`take` on
    the app task); `_head` is written by the consumer alone and `_tail` by the
    producer alone. A second producer needs a critical section first;
  - the payload is written before `_tail` is released and read after it is
    acquired, and one slot is left empty so full and empty are told apart;
  - the queue never blocks: full is `drop(Drop::NoRoom)`, because the link task
    may not wait on the widgets — a stalled read overruns the UART's buffer.
  `Frame::_dropped` is the one count read across the cores; it is advisory and
  `volatile`. `Link`'s counters are not cross-core, and neither is the inbox:
  `feed` and `apply` both run on the app task, and every widget call is inside
  `esp_lv_adapter_lock`.
- `main/drivers/display/nv3007_lcd.cpp` is a hand-written `esp_lcd` panel for
  the NV3007: window-and-write over the power-up sequence in
  `nv3007_init.{h,cpp}`, which is a table and nothing else. The adapter already
  byte-swaps RGB565 and passes exclusive end coordinates to `draw_bitmap`; do
  not swap again in the driver.
- `main/conf/board.h` holds the panel pins and sizes. Native panel is 156x428,
  mounted sideways; MADCTL (`0x36 = 0x60`, last step of the init sequence)
  turns it so the UI surface is 428x156. The adapter does not rotate SPI
  panels, so do not use `lv_display_set_rotation`.
- The board's own firmware answers with the same shape in its `src/wire/`
  (`frame`, `dispatch`, one file a domain): a frame layer that knows no
  message, a dispatcher that knows every arm, and handlers that know one. The
  two `frame` files are the same state machine; keep them that way, and keep
  the one difference the coprocessor has on purpose: a zero-length payload is
  dropped (`Drop::Length`) because an idle line reads as zeroes.
- `main/ui/model.h` `Model` is the seam for the `ThetaGP.PB` protocol, which is
  **not wired in yet**. Protocol code should call the `Menu::set*` / navigation
  functions rather than touch widgets; a setter that really changes a value
  brings the screen up to date through `Menu::refresh()`, the one place that
  decides whether the page is repainted in place or rebuilt.
- `main/ui/spring.h` is the one place a damped ("springy") animation comes
  from. A page calls `Spring::run(key, tuning, from, to, apply, done)`; the
  position at each frame is `1 - e^(-decay t)(cos(omega t) + decay/omega
  sin(omega t))` and nothing else. Tunings (`Spring::CARDS`, `Spring::COLUMN`)
  live in that header; do not write another easing curve in a page.
- `simulate/` is a separate CMake project that compiles the real `../main/ui/`
  sources. ESP-IDF calls are satisfied by stubs in `simulate/include/driver/`;
  `gpio_get_level` always returns idle, so the board button does nothing there
  — keyboard drives navigation.
- The simulator draws straight into SDL and never compiles
  `nv3007_lcd.cpp` / `lvgl_display.cpp` / `button.cpp` / `link.cpp`; it
  validates the UI, not the SPI panel driver, the adapter (including the RGB565
  byte swap) or the wire.

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
- `simulate/CMakeLists.txt` names every `../main/ui/` source for both of its
  targets (`simulate`, `compare`). New UI code must be added there too, with
  stubs under `simulate/include/` for any ESP-IDF header it reaches.

## Conventions

- Every source file begins with the GPLv3 comment header; new files should too.
- C++17, 2-space indent, ~80 columns, namespaces under `ThetaGP::`
  (`Drivers::Display`, `Ui`, `Ui::Theme`).
- No linter, formatter config or CI exists. Verification is three builds and
  two host programs:
  - `cmake --preset esp32s3` + `--build`, and `simulate`'s build;
  - `simulate/build/compare` prints one frame hash a step of a fixed script.
    To compare a refactor, build the same program against another commit
    (`git worktree add`) and diff the two outputs: identical output is the
    claim that the picture did not change;
  - `simulate/build/frame_test` checks the wire: a payload written and read
    back at every length the prefix changes at, the queue's order and its
    overflow, the idle half frame, and each reason a frame is dropped.
- Commit history follows Conventional Commits (`feat:`, `build:`, `refactor:`,
  `docs:`, `chore:`, with optional scopes).
