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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.
#
# If not, see <https://www.gnu.org/licenses/>.
]]

# nanopb is not carried in this repository. It is fetched once, at configure
# time, at the release this firmware's codec is generated against: the codec
# and the runtime that decodes it are one release, so the pin names both.
#
# It lands outside lib/, which is where ESP-IDF looks for components: nanopb is
# a library this firmware compiles a few files of, not a component of its own.

set(THETAGP_NANOPB_REPOSITORY "https://github.com/nanopb/nanopb.git" CACHE STRING
    "The nanopb repository the codec is generated against")
# The nanopb-0.4.9.3 release.
set(THETAGP_NANOPB_TAG "93550bb52a6216a7396d0a168a42b3a88223d811" CACHE STRING
    "The nanopb commit the codec is generated against")

# Fetches nanopb into <dir> unless it is already there, and leaves its path in
# THETAGP_NANOPB_DIR.
function(thetagp_nanopb dir)
  set(THETAGP_NANOPB_DIR "${dir}" PARENT_SCOPE)

  # A source tree that is already there is used as it is, with or without a
  # .git: a checkout copied in by hand needs no network at all.
  if(EXISTS "${dir}/pb.h" AND NOT EXISTS "${dir}/.git")
    message(STATUS "nanopb: using the source tree in ${dir}")
    return()
  endif()

  find_package(Git REQUIRED)

  if(EXISTS "${dir}/.git")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${dir}" rev-parse HEAD
      OUTPUT_VARIABLE current
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE rev_result)
    if(rev_result EQUAL 0 AND current STREQUAL THETAGP_NANOPB_TAG)
      message(STATUS "nanopb: ${dir} is at ${THETAGP_NANOPB_TAG}")
      return()
    endif()
    message(STATUS "nanopb: ${dir} is elsewhere, moving to ${THETAGP_NANOPB_TAG}")
  else()
    file(REMOVE_RECURSE "${dir}")
    file(MAKE_DIRECTORY "${dir}")
    execute_process(
      COMMAND ${GIT_EXECUTABLE} init --quiet "${dir}"
      RESULT_VARIABLE init_result
      ERROR_VARIABLE init_error)
    if(NOT init_result EQUAL 0)
      message(FATAL_ERROR "nanopb: cannot create ${dir}: ${init_error}")
    endif()
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${dir}" remote add origin
              "${THETAGP_NANOPB_REPOSITORY}"
      ERROR_QUIET)
  endif()

  message(STATUS "nanopb: fetching ${THETAGP_NANOPB_TAG} into ${dir}")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${dir}" fetch --quiet --depth 1 origin
            "${THETAGP_NANOPB_TAG}"
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
        "nanopb: fetching ${THETAGP_NANOPB_TAG} from "
        "${THETAGP_NANOPB_REPOSITORY} failed: ${fetch_error}")
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${dir}" checkout --quiet FETCH_HEAD
    RESULT_VARIABLE checkout_result
    ERROR_VARIABLE checkout_error)
  if(NOT checkout_result EQUAL 0)
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR "nanopb: checking out ${THETAGP_NANOPB_TAG} failed: "
                        "${checkout_error}")
  endif()

  if(NOT EXISTS "${dir}/pb.h")
    file(REMOVE_RECURSE "${dir}")
    message(FATAL_ERROR "nanopb: ${dir} came back without a pb.h")
  endif()
endfunction()
