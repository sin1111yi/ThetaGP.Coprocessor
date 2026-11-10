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

# SDL2 is not carried in this repository either. The host build asks for it
# only when this machine has no SDL2 of its own: a CMake package or a
# pkg-config module is used when it is there, and otherwise it is fetched into
# lib/sdl2 at the commit below.
#
# It is fetched the same way as LVGL in cmake/lvgl.cmake, so both dependencies
# are managed by one git workflow.

set(THETAGP_SDL2_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    CACHE STRING "The SDL2 repository the simulator is written against")
# The v2.32.10 release.
set(THETAGP_SDL2_TAG "5d249570393f7a37e037abf22cd6012a4cc56a71" CACHE STRING
    "The SDL2 commit the simulator is written against")

# Fetches SDL2 into <dir> unless it is already there, and leaves its path in
# THETAGP_SDL2_DIR.
function(thetagp_sdl2 dir)
  set(THETAGP_SDL2_DIR "${dir}" PARENT_SCOPE)

  find_package(Git REQUIRED)

  # A checkout already on the pinned commit is used as it is, and one on
  # another commit is moved to it: changing the pin is enough, nothing here
  # has to be removed by hand.
  if(EXISTS "${dir}/.git")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${dir}" rev-parse HEAD
      OUTPUT_VARIABLE current
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE rev_result)
    if(rev_result EQUAL 0 AND current STREQUAL THETAGP_SDL2_TAG)
      message(STATUS "SDL2: ${dir} is at ${THETAGP_SDL2_TAG}")
      return()
    endif()
    message(STATUS "SDL2: ${dir} is elsewhere, moving to ${THETAGP_SDL2_TAG}")
  else()
    file(REMOVE_RECURSE "${dir}")
    file(MAKE_DIRECTORY "${dir}")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} init --quiet "${dir}"
      RESULT_VARIABLE init_result
      ERROR_VARIABLE init_error)
    if(NOT init_result EQUAL 0)
      message(FATAL_ERROR "SDL2: cannot create ${dir}: ${init_error}")
    endif()
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${dir}" remote add origin
              "${THETAGP_SDL2_REPOSITORY}"
      ERROR_QUIET)
  endif()

  message(STATUS "SDL2: fetching ${THETAGP_SDL2_TAG} into ${dir}")

  # One commit is all that is wanted. A server that refuses to hand out a
  # single commit is asked for the history instead, which still lands on the
  # pinned one.
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${dir}" fetch --quiet --depth 1 origin
            "${THETAGP_SDL2_TAG}"
    RESULT_VARIABLE fetch_result
    ERROR_VARIABLE fetch_error)
  if(NOT fetch_result EQUAL 0)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${dir}" fetch --quiet origin
      RESULT_VARIABLE fetch_result
      ERROR_VARIABLE fetch_error)
  endif()
  if(NOT fetch_result EQUAL 0)
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR
        "SDL2: fetching ${THETAGP_SDL2_TAG} from "
        "${THETAGP_SDL2_REPOSITORY} failed: ${fetch_error}")
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${dir}" checkout --quiet FETCH_HEAD
    RESULT_VARIABLE checkout_result
    ERROR_VARIABLE checkout_error)
  if(NOT checkout_result EQUAL 0)
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR "SDL2: checking out ${THETAGP_SDL2_TAG} failed: "
                        "${checkout_error}")
  endif()

  if(NOT EXISTS "${dir}/CMakeLists.txt" OR NOT EXISTS "${dir}/include/SDL.h")
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR "SDL2: ${dir} came back without an SDL2 source tree")
  endif()
endfunction()
