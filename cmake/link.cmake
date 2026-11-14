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

# The codec of Link.proto — the schema the two boards speak — is generated at
# configure time from the schema checkout and the nanopb one, into a directory
# this build compiles and git ignores. It needs protoc, the nanopb generator
# and a python-protobuf runtime on the machine; one that is missing, or a
# failure, stops the configure rather than compiling a stale or absent codec.

function(thetagp_link_codec schema_dir nanopb_dir out_dir)
  # The interpreter is the system's before it is whatever PATH offers first:
  # an ESP-IDF environment puts its own venv at the front of PATH, and that
  # one has no python-protobuf. The first candidate that can import it wins;
  # none does, the configure stops.
  # PATH alone, the way generate.py looks for it: a protoc that CMake finds
  # and the generator does not is a configure that passes and then fails.
  find_program(THETAGP_PROTOC protoc
               NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
  if(NOT THETAGP_PROTOC)
    message(FATAL_ERROR
        "The link codec needs protoc, and it is not on PATH. Install the "
        "protobuf compiler (Arch: protobuf).")
  endif()

  set(candidates "")
  foreach(name python3 python)
    find_program(candidate_system NAMES ${name}
                 PATHS /usr/bin /usr/local/bin NO_DEFAULT_PATH)
    find_program(candidate_path NAMES ${name})
    foreach(found "${candidate_system}" "${candidate_path}")
      if(found AND NOT found MATCHES "-NOTFOUND$")
        list(APPEND candidates "${found}")
      endif()
    endforeach()
    unset(candidate_system CACHE)
    unset(candidate_path CACHE)
  endforeach()
  list(REMOVE_DUPLICATES candidates)

  set(python "")
  foreach(candidate IN LISTS candidates)
    execute_process(
      COMMAND "${candidate}" -c "import google.protobuf"
      RESULT_VARIABLE probe_result
      OUTPUT_QUIET ERROR_QUIET)
    if(probe_result EQUAL 0)
      set(python "${candidate}")
      break()
    endif()
  endforeach()
  if(NOT python)
    message(FATAL_ERROR
        "The link codec needs python-protobuf, and none of these Python "
        "interpreters can import google.protobuf: ${candidates}. Install it "
        "for the system Python (Arch: python-protobuf).")
  endif()
  message(STATUS "link codec: protoc ${THETAGP_PROTOC}, python ${python}")

  set(generator "${schema_dir}/generate.py")
  if(NOT EXISTS "${generator}")
    message(FATAL_ERROR
        "${generator} is not there: lib/ThetaGP.PB is the schema checkout this "
        "firmware speaks, and the codec is generated from it.")
  endif()

  set(argv "${python}" "${generator}" c
           --schema "protocol/Link.proto"
           --nanopb "${nanopb_dir}"
           --out "${out_dir}")
  message(STATUS "link codec: generating ${out_dir}/Link.pb.c ...")
  execute_process(COMMAND ${argv}
                  RESULT_VARIABLE link_result
                  OUTPUT_VARIABLE link_output
                  ERROR_VARIABLE link_error)
  if(NOT link_result EQUAL 0)
    message(FATAL_ERROR
        "Generating the link codec failed: ${generator} exited "
        "${link_result}.\n${link_output}\n${link_error}\n"
        "The nanopb checkout it was given is ${nanopb_dir}.")
  endif()

  foreach(pair "Link.pb.c" "Link.pb.h")
    if(NOT EXISTS "${out_dir}/${pair}")
      message(FATAL_ERROR
          "${generator} exited 0 but ${out_dir}/${pair} is not there.")
    endif()
  endforeach()
endfunction()
