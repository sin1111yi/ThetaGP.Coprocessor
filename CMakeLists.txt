#[[
# This file is a part of ThetaGP.
#
# ThetaGP is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ThetaGP is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.
#
# If not, see <https://www.gnu.org/licenses/>.
]]

cmake_minimum_required(VERSION 3.16)

# Every library this firmware builds against is pinned and fetched by
# lib/CMakeLists.txt. The ESP-IDF component manager would be a second place
# that decides what gets downloaded, so it is switched off; this has to be
# said before project.cmake is read.
set(ENV{IDF_COMPONENT_MANAGER} 0)

# The board is the ESP32-S3 that drives the coprocessor's panel; the toolchain
# file the presets pass is the one that names it.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# The libraries are fetched at configure time rather than carried in the
# tree, and their directories have to exist before project() looks for
# components, so it happens here rather than in a component of its own.
include("${CMAKE_SOURCE_DIR}/lib/CMakeLists.txt")
thetagp_require(lvgl nanopb esp_lcd_touch esp_lvgl_adapter)

# The ones that are ESP-IDF components are named one by one: lib/ itself has
# a CMakeLists.txt, and holds trees (nanopb, the schema) that are not
# components.
set(EXTRA_COMPONENT_DIRS
    "${THETAGP_LVGL_DIR}"
    "${THETAGP_ESP_LCD_TOUCH_DIR}"
    "${THETAGP_ESP_LVGL_ADAPTER_DIR}")

# The protocol between the two boards: the codec of Link.proto is generated
# from the schema checkout with the nanopb one beside it. It is needed before
# project() reads the component below.
include("${CMAKE_SOURCE_DIR}/cmake/link.cmake")
thetagp_link_codec("${CMAKE_SOURCE_DIR}/lib/ThetaGP.PB" "${THETAGP_NANOPB_DIR}"
                   "${CMAKE_SOURCE_DIR}/main/pb")

project(ThetaGP.Coprocessor)
