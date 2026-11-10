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

# The host build of the menu: LVGL compiles for this machine, ui/menu.cpp is the
# same source the firmware links, and the window is the panel.
cmake_minimum_required(VERSION 3.16)

project(thetagp_simulate C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# LVGL is fetched, not carried: see cmake/lvgl.cmake.
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/lvgl.cmake)
thetagp_lvgl(${CMAKE_CURRENT_SOURCE_DIR}/../.deps/lvgl)
add_subdirectory(${THETAGP_LVGL_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

# The window the panel is blitted into. SDL2 ships as a CMake package on most
# systems and as a pkg-config module on the rest.
find_package(SDL2 QUIET)
if(SDL2_FOUND AND TARGET SDL2::SDL2)
  set(THETAGP_SDL2 SDL2::SDL2)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  set(THETAGP_SDL2 PkgConfig::SDL2)
endif()

add_executable(simulate
  main_sim.cpp
  ../main/ui/menu.cpp
)

# The board's GPIO calls are answered by the stubs under include/, and the
# firmware's headers are reached the way they are inside the component: from
# main/.
target_include_directories(simulate PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_compile_definitions(lvgl PUBLIC
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")
target_compile_definitions(simulate PRIVATE
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")

target_link_libraries(simulate PRIVATE lvgl ${THETAGP_SDL2})
if(NOT MSVC)
  # MSVC has no separate libm; the C runtime carries it.
  target_link_libraries(simulate PRIVATE m)
endif()
