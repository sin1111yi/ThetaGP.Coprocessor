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

// The one place a move on this panel gets its spring from. Every animation
// that settles instead of stopping dead is the same motion: a damped
// oscillation let go one whole move away from where it is going,
//
//   x(t) = 1 - e^(-decay t) * (cos(omega t) + (decay / omega) * sin(omega t))
//
// which leaves at rest, runs a little past the place and comes back to it.
// A page says where the move starts and ends and what to do with a position;
// the position at each frame is worked out here, from that function alone.

#pragma once

#include <cstdint>

namespace ThetaGP::Ui::Spring {

// How one kind of move settles. `decay` is how fast the swing dies, `omega`
// how fast it swings, both over the whole of the move (t runs 0 to 1), and
// `durationMs` is how long the move is given on the clock.
struct Tuning {
  float decay;
  float omega;
  uint32_t durationMs;
};

// The cards of a level's left column, and the thumb that follows them: a
// soft spring, about an eighth of a card past the place.
inline constexpr Tuning CARDS = {6.0f, 9.0f, 480};

// The lines of the right column: stiffer, and at rest sooner.
inline constexpr Tuning COLUMN = {9.0f, 11.0f, 480};

// Where the move is at `t` (0 at its start, 1 at its end), as the share of
// the way covered: 0 at the start, past 1 while it overshoots, settling on 1.
float position(const Tuning &tuning, float t);

// What a page does with a position, and what it does once the move is over.
using Apply = void (*)(void *key, float value);
using Done = void (*)(void *key);

// Starts a move from `from` to `to`. `key` names what is moving -- one move
// per key, a new one replacing the one in flight -- and is handed back to
// `apply` at every frame with the value the function gives. The last frame is
// `to` exactly, and `done`, when there is one, follows it.
//
// Must be called where LVGL may be called.
void run(void *key, const Tuning &tuning, float from, float to, Apply apply,
         Done done = nullptr);

// Drops the move `key` has in flight, where it stands: nothing is applied and
// `done` is not called.
void stop(void *key);

} // namespace ThetaGP::Ui::Spring
