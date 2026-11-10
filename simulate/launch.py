#!/usr/bin/env python3
"""Build the host build of the menu and run it.

One entry point on every platform: it reads what this machine is, finds CMake
and a compiler, builds `simulate/` (fetching SDL2 when the machine has none),
then runs it. A machine without the toolchain is told what is missing rather
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
    """Compiler overrides for CMake, and how to describe them."""
    if sys.platform.startswith("win"):
        # The clang that CMake picks up on Windows has no Windows SDK to
        # link against; zig carries its own C runtime and headers, which
        # keeps this build working on a machine without MSVC.
        zig = find(["zig"])
        if zig and find(["cl"]) is None:
            flags = ["-DCMAKE_C_COMPILER=" + zig,
                     "-DCMAKE_C_COMPILER_ARG1=cc",
                     "-DCMAKE_CXX_COMPILER=" + zig,
                     "-DCMAKE_CXX_COMPILER_ARG1=c++",
                     "-DCMAKE_ASM_COMPILER=" + zig,
                     "-DCMAKE_ASM_COMPILER_ARG1=cc"]
            return flags, "zig cc/c++"
        return [], "MSVC (CMake default)"
    return [], find(["cc", "gcc", "clang"]) or "CMake default"


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
        description="Build the host build of the menu and run it.")
    parser.add_argument("--clean", action="store_true",
                        help="throw the build tree away first")
    args, rest = parser.parse_known_args()
    # Everything after the separator goes to the binary, which does not
    # expect the separator itself.
    if rest and rest[0] == "--":
        rest = rest[1:]

    cmake = find(["cmake"])
    if cmake is None:
        print("simulate: cmake is not on PATH. Install CMake, or add it: "
              "https://cmake.org/download/")
        return 1

    flags, multiconfig = generator()
    compiler_flags, compiler_name = compiler()
    print("simulate: %s %s, %s, %s" %
          (system(), platform.machine() or "?", compiler_name,
           flags[1] if flags else "CMake default"), flush=True)

    if args.clean:
        shutil.rmtree(BUILD, ignore_errors=True)

    configure = [cmake, "-S", HERE, "-B", BUILD,
                 "-DCMAKE_BUILD_TYPE=Release"] + flags + compiler_flags
    if run(configure) != 0:
        return 1

    # The simulator is the only thing wanted: LVGL's examples and demos are
    # separate targets and are not built.
    build = [cmake, "--build", BUILD, "--target", "simulate", "--parallel"]
    if multiconfig:
        build += ["--config", "Release"]
    if run(build) != 0:
        return 1

    return subprocess.call([binary(multiconfig)] + rest)


if __name__ == "__main__":
    sys.exit(main())
