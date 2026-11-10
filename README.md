# ThetaGP.Coprocessor

<p align="center">
  <img src="asset/thetagp-logo.png" alt="ThetaGP Logo" width="400">
</p>

The panel half of ThetaGP: an ESP32-S3 that drives the display mounted across
the board. A 156x428 NV3007 sits on its side, so LVGL draws the menu on the
quarter turn of it, 428x156, and everything on screen is a card, a rule or an
arc drawn from a flat palette.

This firmware is not the gamepad's own and never touches USB HID. What it shows
is meant to come from the protocol in `ThetaGP.PB`; that link is not wired in
yet, so the pages read the model `menu.h` exposes, and `main.c` fills it with
the values the display was built against.

## Features

- ESP32-S3 at 240 MHz, ESP-IDF v6.1
- NV3007 panel, 156x428 native, drawn at 428x156 after a quarter turn, inside a
  420x140 window
- LVGL v9.5 (submodule), 16 bpp RGB565, FreeRTOS support, 10 ms refresh period
- SPI at 60 MHz with the chip select held across a command and its parameters;
  the panel takes RGB565 most significant byte first
- A column of cards that scrolls under the top edge, with an arc in the right
  third tracking how far down the column the mark sits
- Five pages behind the menu, read-only for now: status, profiles, keymap,
  config, about
- BOOT (GPIO0) drives the menu until the protocol brings buttons of its own: a
  short press moves the mark, a long press enters or leaves a page
- A host build under `sim/` that runs the same `menu.c` and streams it to a
  browser, for when the board is not on the desk

## Layout

```
main/main.c            display bring-up, SPI, the LVGL loop
main/menu.c            pages, the card column, the arc, the board button
main/menu.h            the model the menu shows and the calls that fill it
main/board.h           pins, panel size, SPI clock
sim/                   the same menu on the host, served over HTTP
components/lvgl        LVGL, a submodule pinned to its commit
```

## Build

The project is an ESP-IDF project; the presets only add a tree per build type.

```bash
git submodule update --init
. ~/.espressif/tools/activate_idf_v6.1.sh    # or: . "$IDF_PATH/export.sh"
cmake --preset esp32s3-release
cmake --build --preset esp32s3-release
```

`esp32s3` keeps the debug build, `esp32s3-release` optimises it, and each tree
lands under `build/<preset>/`. `sdkconfig.defaults` carries the settings the
panel needs: the NV3007 driver, the fonts, the refresh period, FreeRTOS support
and the CPU clock. `sdkconfig` is generated from it and stays out of the tree.

## Flash

The port is the chip's own USB serial/JTAG, not the debug probe.

```bash
idf.py -B build/esp32s3-release -p /dev/ttyACM0 flash
```

The firmware answers on that port too: the boot banner and `nv3007: 156x428
ready` are how a panel that failed to come up says so.

## Simulator

`sim/` compiles `main/menu.c` against a host LVGL, with the board's GPIO calls
stubbed out. Frames are published to a shared file and served as an MJPEG
stream, so the menu can be watched from any browser that can reach the machine.

```bash
sim/run.sh                          # http://<host>:8080/
sim/run.sh --port 8080 --fps 20     # the flags go to stream.py
```

The page carries three buttons (next, enter, back) that are forwarded to
the simulator's stdin. Frames are indexed PNGs of the 428x156 surface, sent only
when the picture changes, so a still screen costs nothing. It shows the layout
and the motion; it says nothing about the panel's timing or the real frame
rate.

## License

GPL-3.0, the same as the rest of ThetaGP. See `LICENSE`.
