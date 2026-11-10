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

# LVGL is not carried in this repository. It is fetched once, at configure
# time, into a directory outside the tree, at the commit this firmware was
# written against: the panel's driver and the Kconfig names the build sets are
# properties of that commit, so it is pinned rather than followed.
#
# The directory is shared by both builds here (the ESP-IDF one and the host
# one), so a second configure costs nothing.

set(THETAGP_LVGL_REPOSITORY "https://github.com/lvgl/lvgl.git" CACHE STRING
    "The LVGL repository the firmware is written against")
set(THETAGP_LVGL_TAG "3be31ace230de3b57347f8d678ff32e8ac1de7dc" CACHE STRING
    "The LVGL commit the firmware is written against")

# Fetches LVGL into <dir> unless it is already there, and leaves its path in
# THETAGP_LVGL_DIR.
function(thetagp_lvgl dir)
  set(THETAGP_LVGL_DIR "${dir}" PARENT_SCOPE)

  if(EXISTS "${dir}/CMakeLists.txt")
    message(STATUS "LVGL: ${dir} is already fetched")
    return()
  endif()

  find_package(Git REQUIRED)
  message(STATUS "LVGL: fetching ${THETAGP_LVGL_TAG} into ${dir}")

  file(MAKE_DIRECTORY "${dir}")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} init --quiet "${dir}"
    RESULT_VARIABLE init_result
    ERROR_VARIABLE init_error)
  if(NOT init_result EQUAL 0)
    message(FATAL_ERROR "LVGL: cannot create ${dir}: ${init_error}")
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${dir}" remote add origin
            "${THETAGP_LVGL_REPOSITORY}"
    ERROR_QUIET)

  # One commit is all that is wanted. A server that refuses to hand out a
  # single commit is asked for the history instead, which still lands on the
  # pinned one.
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${dir}" fetch --quiet --depth 1 origin
            "${THETAGP_LVGL_TAG}"
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
        "LVGL: fetching ${THETAGP_LVGL_TAG} from "
        "${THETAGP_LVGL_REPOSITORY} failed: ${fetch_error}")
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${dir}" checkout --quiet FETCH_HEAD
    RESULT_VARIABLE checkout_result
    ERROR_VARIABLE checkout_error)
  if(NOT checkout_result EQUAL 0)
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR "LVGL: checking out ${THETAGP_LVGL_TAG} failed: "
                        "${checkout_error}")
  endif()

  if(NOT EXISTS "${dir}/CMakeLists.txt")
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR "LVGL: ${dir} came back without a CMakeLists.txt")
  endif()
endfunction()
