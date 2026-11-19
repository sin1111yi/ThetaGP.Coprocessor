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

idf_component_register(
    SRCS "main.cpp"
         "app/Coprocessor.cpp"
         "drivers/display/lvgl_display.cpp"
         "drivers/display/nv3007_init.cpp"
         "drivers/display/nv3007_lcd.cpp"
         "drivers/input/button.cpp"
         "drivers/link/frame.cpp"
         "drivers/link/link.cpp"
         "protocol/panel_protocol.cpp"
         "pb/Link.pb.c"
         "${THETAGP_NANOPB_DIR}/pb_common.c"
         "${THETAGP_NANOPB_DIR}/pb_encode.c"
         "${THETAGP_NANOPB_DIR}/pb_decode.c"
         "ui/assets/logo.c"
         "ui/menu.cpp"
         "ui/pages/home.cpp"
         "ui/profile/profile.cpp"
         "ui/spring.cpp"
         "ui/widgets/column.cpp"
    INCLUDE_DIRS "." "pb" "${THETAGP_NANOPB_DIR}"
    REQUIRES lvgl esp_timer esp_driver_spi esp_driver_gpio esp_driver_uart
             esp_lcd espressif__esp_lvgl_adapter)

# nanopb and the adapter come from lib/CMakeLists.txt, not from a manifest:
# there is no idf_component.yml here, and the component manager is off.

# The panel is built on esp_lcd's panel interface, whose header is the
# component's own rather than one of the exported ones.
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${IDF_PATH}/components/esp_lcd/interface")

# The adapter's default-config macros leave fields to the compiler: that is
# their shape, not a mistake in a line here.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers -Wno-error=missing-field-initializers)
