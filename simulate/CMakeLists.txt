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

# The libraries are fetched, not carried: lib/CMakeLists.txt pins them for
# this build and the firmware's alike.
include(${CMAKE_CURRENT_SOURCE_DIR}/../lib/CMakeLists.txt)
thetagp_require(lvgl)
add_subdirectory(${THETAGP_LVGL_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

# The window the panel is blitted into. A checkout under lib/ is what was put
# there by hand and wins over anything installed on the machine; otherwise a
# system SDL2 is used when there is one, and fetched into lib/ only when there
# is not.
set(THETAGP_SDL2_DIR ${THETAGP_LIB_DIR}/sdl2)
if(EXISTS "${THETAGP_SDL2_DIR}/CMakeLists.txt"
   AND EXISTS "${THETAGP_SDL2_DIR}/include/SDL.h")
  set(THETAGP_SDL2_BUNDLED TRUE)
else()
  set(THETAGP_SDL2_BUNDLED FALSE)
  find_package(SDL2 QUIET)
  if(SDL2_FOUND AND TARGET SDL2::SDL2)
    set(THETAGP_SDL2 SDL2::SDL2)
  else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()

    if(TARGET PkgConfig::SDL2)
      set(THETAGP_SDL2 PkgConfig::SDL2)
    else()
      thetagp_require(sdl2)
      set(THETAGP_SDL2_BUNDLED TRUE)
    endif()
  endif()
endif()

if(THETAGP_SDL2_BUNDLED)
  # The simulator has no use for a shared object or the test programs, and
  # wants its own main (SDL_MAIN_HANDLED below).
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

  # SDL2 2.32.10 still declares CMake 3.0, which CMake 4 refuses to
  # configure unless the floor is raised.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

  add_subdirectory(${THETAGP_SDL2_DIR} ${CMAKE_CURRENT_BINARY_DIR}/sdl2)
  # The alias also points at the static library when no shared one is built.
  set(THETAGP_SDL2 SDL2::SDL2)
endif()

add_executable(simulate
  main_sim.cpp
  ../main/ui/assets/logo.c
  ../main/ui/menu.cpp
  ../main/ui/model.cpp
  ../main/ui/pages/about.cpp
  ../main/ui/pages/home.cpp
  ../main/ui/pages/mapping.cpp
  ../main/ui/pages/monitor.cpp
  ../main/ui/pages/options.cpp
  ../main/ui/pages/pages.cpp
  ../main/ui/pages/profiles.cpp
  ../main/ui/profile/profile.cpp
  ../main/ui/splash.cpp
  ../main/ui/spring.cpp
  ../main/ui/widgets/column.cpp
)

# The board's GPIO calls are answered by the stubs under include/, and the
# firmware's headers are reached the way they are inside the component: from
# main/.
target_include_directories(simulate PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# The same sources without the window: one hash a frame, for comparing two
# trees on the picture they draw rather than on how they are written.
add_executable(compare
  compare.cpp
  ../main/ui/assets/logo.c
  ../main/ui/menu.cpp
  ../main/ui/model.cpp
  ../main/ui/pages/about.cpp
  ../main/ui/pages/home.cpp
  ../main/ui/pages/mapping.cpp
  ../main/ui/pages/monitor.cpp
  ../main/ui/pages/options.cpp
  ../main/ui/pages/pages.cpp
  ../main/ui/pages/profiles.cpp
  ../main/ui/profile/profile.cpp
  ../main/ui/splash.cpp
  ../main/ui/spring.cpp
  ../main/ui/widgets/column.cpp
)
target_include_directories(compare PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)
target_compile_definitions(compare PRIVATE
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")
target_link_libraries(compare PRIVATE lvgl)
if(NOT MSVC)
  target_link_libraries(compare PRIVATE m)
endif()

# The wire's framing, with no board under it: a payload written and read back,
# and the frames that must not be taken.
add_executable(frame_test
  frame_test.cpp
  ../main/drivers/link/frame.cpp
)
target_include_directories(frame_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_compile_definitions(lvgl PUBLIC
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")
target_compile_definitions(simulate PRIVATE
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h"
  SDL_MAIN_HANDLED)

target_link_libraries(simulate PRIVATE lvgl ${THETAGP_SDL2})
if(NOT MSVC)
  # MSVC has no separate libm; the C runtime carries it.
  target_link_libraries(simulate PRIVATE m)
endif()
