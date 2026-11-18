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

#include "ui/spring.h"

#include <cmath>

#include "lvgl.h"

namespace ThetaGP::Ui::Spring {

namespace {

// LVGL animates integers, and skips a frame whose integer did not change: the
// share of the way is carried in sixteen fractional bits, which is finer than
// any pixel a move here covers.
constexpr int32_t kOne = 1 << 16;

// What one move in flight remembers. It lives as long as LVGL's animation
// does, and is freed when LVGL lets that go, however it ends.
struct Move {
  Tuning tuning;
  float from;
  float to;
  Apply apply;
  Done done;
};

Move *moveOf(const lv_anim_t *anim) {
  return static_cast<Move *>(lv_anim_get_user_data(anim));
}

int32_t path(const lv_anim_t *anim) {
  const Move *move = moveOf(anim);
  const float t = anim->duration > 0
                      ? (float)anim->act_time / (float)anim->duration
                      : 1.0f;
  return (int32_t)(position(move->tuning, t) * (float)kOne);
}

void frame(lv_anim_t *anim, int32_t value) {
  const Move *move = moveOf(anim);
  const float share = (float)value / (float)kOne;
  move->apply(anim->var, move->from + (move->to - move->from) * share);
}

void completed(lv_anim_t *anim) {
  const Move *move = moveOf(anim);
  // The function is within a hair of 1 at the end, not on it: the place the
  // move was going to is given exactly.
  move->apply(anim->var, move->to);
  if (move->done != nullptr) {
    move->done(anim->var);
  }
}

void deleted(lv_anim_t *anim) { lv_free(moveOf(anim)); }

} // namespace

float position(const Tuning &tuning, float t) {
  if (t <= 0.0f) {
    return 0.0f;
  }
  if (tuning.omega <= 0.0f) {
    // No swing to speak of: the move simply decays onto its place.
    return 1.0f - std::exp(-tuning.decay * t);
  }
  const float phase = tuning.omega * t;
  return 1.0f - std::exp(-tuning.decay * t) *
                    (std::cos(phase) +
                     tuning.decay / tuning.omega * std::sin(phase));
}

void run(void *key, const Tuning &tuning, float from, float to, Apply apply,
         Done done) {
  if (key == nullptr || apply == nullptr) {
    return;
  }
  stop(key);

  Move *move = static_cast<Move *>(lv_malloc(sizeof(Move)));
  if (move == nullptr) {
    // No room to remember the move: it is made at once rather than lost.
    apply(key, to);
    if (done != nullptr) {
      done(key);
    }
    return;
  }
  *move = {tuning, from, to, apply, done};

  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, key);
  lv_anim_set_user_data(&anim, move);
  lv_anim_set_values(&anim, 0, kOne);
  lv_anim_set_duration(&anim, tuning.durationMs);
  lv_anim_set_path_cb(&anim, path);
  lv_anim_set_custom_exec_cb(&anim, frame);
  lv_anim_set_completed_cb(&anim, completed);
  lv_anim_set_deleted_cb(&anim, deleted);
  lv_anim_start(&anim);
}

void stop(void *key) {
  if (key != nullptr) {
    lv_anim_delete(key, nullptr);
  }
}

} // namespace ThetaGP::Ui::Spring
