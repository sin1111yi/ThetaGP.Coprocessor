cmake_minimum_required(VERSION 3.16)

# The host build of the menu: LVGL compiles for the PC, menu.c is the same
# source the firmware links, and the frame lands in a file stream.py serves.
project(thetagp_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../components/lvgl
                 ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

add_executable(sim
  main_sim.c
  ../main/menu.c
)

target_include_directories(sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_compile_definitions(lvgl PUBLIC
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")
target_compile_definitions(sim PRIVATE
  LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h")

target_link_libraries(sim PRIVATE lvgl m)
