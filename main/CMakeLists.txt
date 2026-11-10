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
         "Coprocessor.cpp"
         "drivers/display/nv3007_panel.cpp"
         "drivers/display/lvgl_display.cpp"
         "drivers/link/link.cpp"
         "ui/menu.cpp"
         "ui/logo.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_timer esp_driver_spi esp_driver_gpio esp_driver_uart)
