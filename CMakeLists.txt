cmake_minimum_required(VERSION 3.16)

# The board is the ESP32-S3 that drives the coprocessor's panel; the toolchain
# file the presets pass is the one that names it.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ThetaGP.Coprocessor)
