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

# The board is the ESP32-S3 that drives the coprocessor's panel; the toolchain
# file the presets pass is the one that names it.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# LVGL is fetched at configure time rather than carried in the tree, and its
# directory has to exist before project() looks for components, so it happens
# here rather than in a component of its own.
include("${CMAKE_SOURCE_DIR}/cmake/lvgl.cmake")
thetagp_lvgl("${CMAKE_SOURCE_DIR}/lib/lvgl")
set(EXTRA_COMPONENT_DIRS "${CMAKE_SOURCE_DIR}/lib")

project(ThetaGP.Coprocessor)

# LVGL 9.5.0 carries the NV3007 driver but publishes no Kconfig switch for it,
# so the switch is handed to the component that compiles it. The generic MIPI
# driver it builds on is a Kconfig switch of its own, set in
# sdkconfig.defaults.
if(TARGET __idf_lvgl)
  target_compile_definitions(__idf_lvgl PUBLIC LV_USE_NV3007=1)
endif()
