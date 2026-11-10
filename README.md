# ThetaGP.Coprocessor

<p align="center">
  <img src="asset/thetagp-logo.png" alt="ThetaGP Logo" width="400">
</p>

The panel half of ThetaGP: an ESP32-S3 that drives the display mounted across
the board. A 156x428 NV3007 sits on its side, so LVGL draws the menu on the
quarter turn of it, 428x156: a column of cards that scrolls under the top edge,
an arc in the right third that tracks how far down the column the mark sits, and
the pages behind them, all drawn from a flat palette.

This firmware is not the gamepad's own and never touches USB HID. What it shows
is meant to come from the protocol in `ThetaGP.PB`; that link is not wired in
yet, so the pages read the model `ui/menu.h` exposes, and `Coprocessor::setup`
fills it with the values the display was built against.
