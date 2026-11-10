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

# The host build of the menu: LVGL compiles for the PC, ui/menu.cpp is the same
# source the firmware links, and the frame lands in a file stream.py serves.
cmake_minimum_required(VERSION 3.16)

project(thetagp_simulate C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../components/lvgl
                 ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

add_executable(simulate
  main_sim.cpp
  ../main/ui/menu.cpp
)

# The board's GPIO calls are answered by the stubs under include/, and the
# firmware's headers are reached the way they are inside the tree: from src/.
target_include_directories(simulate PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_compile_definitions(lvgl PUBLIC
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")
target_compile_definitions(simulate PRIVATE
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")

target_link_libraries(simulate PRIVATE lvgl m)
