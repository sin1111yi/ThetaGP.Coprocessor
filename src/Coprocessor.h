/**
 * This file is a part of ThetaGP.
 *
 * ThetaGP is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * ThetaGP is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.
 *
 * If not, see <https://www.gnu.org/licenses/>.
 */

// The firmware itself: bring the panel up, put the menu on it, then hand the
// CPU to LVGL. Everything the coprocessor will ever say arrives through the
// model the menu reads, which the protocol fills.

#pragma once

namespace ThetaGP {

class Coprocessor {
public:
  static Coprocessor &getInstance() {
    static Coprocessor instance;
    return instance;
  }

  void setup();
  void run();
};

} // namespace ThetaGP
