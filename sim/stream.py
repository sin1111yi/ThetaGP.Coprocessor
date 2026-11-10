#!/usr/bin/env python3
"""Serve the host build of the coprocessor's menu as an MJPEG stream.

The simulator (sim/build/sim) draws into a shared frame file; this script
encodes each new frame to JPEG and pushes it to every connected browser.
Navigation keys pressed in the page are forwarded to the simulator's stdin.
"""

import argparse
import io
import mmap
import struct
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

FRAME_MAGIC = 0x54475053
HEADER = struct.Struct("<IIII")
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
  <h1>ThetaGP coprocessor · sim</h1>
  <img class="screen" src="/stream" id="screen" alt="screen">
  <div class="row">
    <button onclick="key('n')">下一张</button>
    <button onclick="key('e')">进入</button>
    <button onclick="key('b')">返回</button>
  </div>
  <p class="hint">自动轮播每秒一次；按钮直接驱动模拟器的菜单。</p>
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


class Frame:
    """The simulator's newest published frame, read without tearing."""

    def __init__(self, path):
        self.lock = threading.Lock()
        self.path = path
        self.map = None
        self.width = 0
        self.height = 0
        self.seq = 0

    def _open(self):
        for _ in range(200):
            try:
                f = open(self.path, "rb")
                size = f.seek(0, io.SEEK_END)
                if size > HEADER.size:
                    self.map = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
                    magic, _, w, h = HEADER.unpack_from(self.map, 0)
                    if magic == FRAME_MAGIC:
                        self.width, self.height = w, h
                        return True
            except OSError:
                pass
            time.sleep(0.1)
        return False

    def wait(self, last, timeout):
        """Blocks until the frame changes, or the timeout passes."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.map is not None:
                s = struct.unpack_from("<I", self.map, 4)[0]
                if s != last and s % 2 == 0:
                    return s
            time.sleep(0.004)
        return None

    def pixels(self):
        size = self.width * self.height * 2
        return self.map[HEADER.size:HEADER.size + size]


def encode(frame, fmt, quality):
    from PIL import Image

    image = Image.frombytes("RGB", (frame.width, frame.height),
                            frame.pixels(), "raw", XRGB565)
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
    frame = None
    sim = None
    quality = 60
    format = "png"
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
        aspect = "{} / {}".format(self.frame.width or 428, self.frame.height or 156)
        body = PAGE.format(aspect=aspect).encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def key(self):
        pressed = ""
        query = self.path.partition("?")[2]
        for part in query.split("&"):
            name, _, value = part.partition("=")
            if name == "k" and value in ("n", "e", "b"):
                pressed = value
        if pressed and self.sim is not None and self.sim.stdin is not None:
            try:
                self.sim.stdin.write(pressed.encode())
                self.sim.stdin.flush()
            except (BrokenPipeError, ValueError):
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
        seen = -1
        last_raw = None
        last_sent = 0.0
        try:
            while True:
                seq = self.frame.wait(seen, 3.0)
                if seq is None:
                    continue
                seen = seq
                now = time.monotonic()
                if now - last_sent < interval:
                    continue
                raw = self.frame.pixels()
                if raw == last_raw:
                    continue
                last_raw = raw
                body, kind = encode(self.frame, self.format, self.quality)
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
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--frame", default="/tmp/opencode/thetagp_frame.bin")
    parser.add_argument("--sim", default="./build/sim")
    parser.add_argument("--no-sim", action="store_true",
                        help="attach to an already running simulator")
    parser.add_argument("--format", choices=("png", "jpeg"), default="png")
    parser.add_argument("--fps", type=float, default=20.0,
                        help="most frames per second sent to a viewer")
    parser.add_argument("--quality", type=int, default=60)
    args = parser.parse_args()

    sim = None
    if not args.no_sim:
        sim = subprocess.Popen([args.sim, args.frame],
                               stdin=subprocess.PIPE,
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)

    frame = Frame(args.frame)
    if not frame._open():
        print("no frames from", args.frame)
        return 1

    Handler.frame = frame
    Handler.sim = sim
    Handler.quality = args.quality
    Handler.format = args.format
    Handler.fps = args.fps

    server = Server((args.host, args.port), Handler)
    print("sim stream on http://{}:{}/".format(args.host, args.port))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        if sim is not None:
            sim.terminate()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
