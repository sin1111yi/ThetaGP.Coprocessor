#!/usr/bin/env python3
"""Build the host build of the menu and serve it.

One entry point on every platform: it reads what this machine is, finds CMake
and a compiler, picks a generator, builds `simulate/`, then hands over to
`stream.py`. A machine without the toolchain is told what is missing rather
than handed a compiler error, because this is the script people run first.
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(HERE)
BUILD = os.path.join(HERE, "build")


def find(names):
    """The first of `names` that this machine has on PATH."""
    for name in names:
        path = shutil.which(name)
        if path:
            return path
    return None


def system():
    return platform.system() or sys.platform


def compiler():
    if sys.platform.startswith("win"):
        return "MSVC (CMake default)"
    return find(["cc", "gcc", "clang"]) or "CMake default"


def generator():
    """A generator, and whether it picks the build type at build time."""
    if find(["ninja"]):
        return ["-G", "Ninja"], False
    if sys.platform.startswith("win"):
        # Visual Studio is the one a Windows box usually has; it is
        # multi-config, so Release is passed to the build instead.
        return [], True
    return [], False


def binary(multiconfig):
    name = "simulate.exe" if sys.platform.startswith("win") else "simulate"
    if multiconfig:
        return os.path.join(BUILD, "Release", name)
    return os.path.join(BUILD, name)


def run(command):
    print("simulate: " + " ".join(command), flush=True)
    return subprocess.call(command)


def main():
    parser = argparse.ArgumentParser(
        description="Build the host build of the menu and serve it.")
    parser.add_argument("--clean", action="store_true",
                        help="throw the build tree away first")
    args, rest = parser.parse_known_args()

    cmake = find(["cmake"])
    if cmake is None:
        print("simulate: cmake is not on PATH. Install CMake, or add it: "
              "https://cmake.org/download/")
        return 1

    if not os.path.exists(os.path.join(TREE, "components", "lvgl",
                                       "CMakeLists.txt")):
        print("simulate: components/lvgl is not there. Run "
              "`git submodule update --init` in %s first." % TREE)
        return 1

    try:
        import PIL  # noqa: F401
    except ImportError:
        print("simulate: Pillow is missing. Run "
              "`%s -m pip install pillow`." % sys.executable)
        return 1

    flags, multiconfig = generator()
    print("simulate: %s %s, %s, %s" %
          (system(), platform.machine() or "?", compiler(),
           flags[1] if flags else "CMake default"), flush=True)

    if args.clean:
        shutil.rmtree(BUILD, ignore_errors=True)

    configure = [cmake, "-S", HERE, "-B", BUILD,
                 "-DCMAKE_BUILD_TYPE=Release"] + flags
    if run(configure) != 0:
        return 1

    build = [cmake, "--build", BUILD, "--parallel"]
    if multiconfig:
        build += ["--config", "Release"]
    if run(build) != 0:
        return 1

    server = [sys.executable, os.path.join(HERE, "stream.py"),
              "--sim", binary(multiconfig)] + rest
    return subprocess.call(server)


if __name__ == "__main__":
    sys.exit(main())
