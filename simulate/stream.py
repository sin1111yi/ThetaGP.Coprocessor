#!/usr/bin/env python3
"""Serve the host build of the menu as an MJPEG stream.

The simulator draws into stdout as a header and a window of RGB565; this script
reads those frames off the pipe, encodes each new one as a PNG and pushes it to
every connected browser. Navigation keys pressed in the page go back down the
simulator's stdin, so the host side stays a pair of pipes.
"""

import argparse
import io
import os
import struct
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

FRAME_MAGIC = 0x54475053
HEADER = struct.Struct("<III")
XRGB565 = "BGR;16"

PAGE = """<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ThetaGP coprocessor</title>
<style>
  body {{ margin: 0; background: #06090d; color: #7c8899;
         font: 14px system-ui, sans-serif; }}
  .wrap {{ max-width: 900px; margin: 0 auto; padding: 16px; }}
  h1 {{ font-size: 15px; font-weight: 500; color: #ecf1f8;
        letter-spacing: .08em; text-transform: uppercase; }}
  .screen {{ width: 100%; aspect-ratio: {aspect}; background: #000;
             border-radius: 10px; border: 1px solid #1b2431;
             image-rendering: pixelated; display: block; }}
  .row {{ display: flex; gap: 10px; margin-top: 14px; flex-wrap: wrap; }}
  button {{ flex: 1 1 120px; padding: 12px 10px; border-radius: 8px;
            border: 1px solid #232e3c; background: #121821; color: #ecf1f8;
            font-size: 15px; }}
  button:active {{ background: #1b2431; border-color: #3ddc97; }}
  .hint {{ margin-top: 12px; font-size: 12px; color: #55606f; }}
</style>
<div class="wrap">
  <h1>ThetaGP coprocessor · simulate</h1>
  <img class="screen" src="/stream" id="screen" alt="screen">
  <div class="row">
    <button onclick="key('n')">下一张</button>
    <button onclick="key('e')">进入</button>
    <button onclick="key('b')">返回</button>
  </div>
  <p class="hint">按键直接驱动模拟器的菜单。</p>
</div>
<script>
  function key(k) {{ fetch('/key?k=' + k).catch(() => {{}}); }}
  // Reopen the stream if the simulator restarts.
  document.getElementById('screen').addEventListener('error', () => {{
    setTimeout(() => {{
      document.getElementById('screen').src = '/stream?t=' + Date.now();
    }}, 1000);
  }});
</script>
"""


class Frames:
    """The newest frame the simulator published, kept off the pipe."""

    def __init__(self):
        self.cond = threading.Condition()
        self.data = b""
        self.seq = 0
        self.width = 428
        self.height = 156
        self.alive = True

    def readFrom(self, pipe):
        while True:
            head = self._read(pipe, HEADER.size)
            if head is None:
                break
            magic, width, height = HEADER.unpack(head)
            if magic != FRAME_MAGIC or width == 0 or height == 0:
                break
            body = self._read(pipe, width * height * 2)
            if body is None:
                break
            with self.cond:
                self.data, self.width, self.height = body, width, height
                self.seq += 1
                self.cond.notify_all()
        with self.cond:
            self.alive = False
            self.cond.notify_all()

    @staticmethod
    def _read(pipe, size):
        chunk = b""
        while len(chunk) < size:
            part = pipe.read(size - len(chunk))
            if not part:
                return None
            chunk += part
        return chunk

    def latest(self, after, timeout):
        """The frame after `after`, or None if the simulator stayed still."""
        deadline = time.monotonic() + timeout
        with self.cond:
            while self.seq == after and self.alive:
                left = deadline - time.monotonic()
                if left <= 0:
                    return None
                self.cond.wait(left)
            if self.seq == after:
                return None
            return self.seq, self.data, self.width, self.height


def encode(data, width, height, fmt, quality):
    from PIL import Image

    image = Image.frombytes("RGB", (width, height), data, "raw", XRGB565)
    out = io.BytesIO()
    if fmt == "jpeg":
        image.save(out, "JPEG", quality=quality)
        return out.getvalue(), "image/jpeg"
    # The panel shows a flat palette, so an indexed PNG is both exact and a
    # third the size of a JPEG of the same frame.
    paletted = image.quantize(colors=64, method=Image.Quantize.MEDIANCUT,
                              dither=Image.Dither.NONE)
    paletted.save(out, "PNG", optimize=True)
    return out.getvalue(), "image/png"


class Server(ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 32


class Handler(BaseHTTPRequestHandler):
    frames = None
    sim = None
    quality = 60
    fmt = "png"
    fps = 20.0

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path.startswith("/stream"):
            self.stream()
        elif self.path.startswith("/key"):
            self.key()
        else:
            self.page()

    def page(self):
        aspect = "{} / {}".format(self.frames.width, self.frames.height)
        body = PAGE.format(aspect=aspect).encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def key(self):
        pressed = ""
        for part in self.path.partition("?")[2].split("&"):
            name, _, value = part.partition("=")
            if name == "k" and value in ("n", "e", "b"):
                pressed = value
        if pressed and self.sim is not None and self.sim.stdin is not None:
            try:
                self.sim.stdin.write(pressed.encode())
                self.sim.stdin.flush()
            except (BrokenPipeError, ValueError, OSError):
                pass
        self.send_response(204)
        self.end_headers()

    def stream(self):
        self.send_response(200)
        self.send_header("Content-Type",
                         "multipart/x-mixed-replace; boundary=frame")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()

        interval = 1.0 / self.fps
        seen = 0
        last_raw = None
        last_sent = 0.0
        try:
            while True:
                got = self.frames.latest(seen, 3.0)
                if got is None:
                    if not self.frames.alive:
                        return
                    continue
                seen, data, width, height = got
                now = time.monotonic()
                if now - last_sent < interval or data == last_raw:
                    continue
                last_raw = data
                body, kind = encode(data, width, height, self.fmt, self.quality)
                last_sent = now
                self.wfile.write(b"--frame\r\n")
                self.wfile.write(b"Content-Type: " + kind.encode() + b"\r\n")
                self.wfile.write(b"Content-Length: %d\r\n\r\n" % len(body))
                self.wfile.write(body)
                self.wfile.write(b"\r\n")
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            return


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--sim", default=os.path.join(here, "build", "simulate"),
                        help="the simulator binary this server drives")
    parser.add_argument("--format", choices=("png", "jpeg"), default="png")
    parser.add_argument("--fps", type=float, default=20.0,
                        help="most frames per second sent to a viewer")
    parser.add_argument("--quality", type=int, default=60,
                        help="JPEG quality, when --format jpeg is chosen")
    args = parser.parse_args()

    try:
        import PIL  # noqa: F401
    except ImportError:
        print("stream: Pillow is missing: python -m pip install pillow")
        return 1

    if not os.path.exists(args.sim):
        print("stream: %s is not there; run launch.py to build it" % args.sim)
        return 1

    sim = subprocess.Popen([args.sim], stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, bufsize=0)
    frames = Frames()
    reader = threading.Thread(target=frames.readFrom, args=(sim.stdout,),
                              daemon=True)
    reader.start()

    Handler.frames = frames
    Handler.sim = sim
    Handler.fmt = args.format
    Handler.fps = args.fps
    Handler.quality = args.quality

    server = Server((args.host, args.port), Handler)
    print("simulate: http://%s:%d/" % (args.host, args.port), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        sim.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())
