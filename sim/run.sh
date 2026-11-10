#!/usr/bin/env bash
# Build the host build of the menu and serve it, so the UI can be watched in a
# browser from anywhere that can reach this machine.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
cmake -S "$here" -B "$here/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$here/build" -j"$(nproc)" >/dev/null
exec python3 "$here/stream.py" "$@"
