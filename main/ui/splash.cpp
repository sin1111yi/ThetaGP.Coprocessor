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

#include "ui/splash.h"

#include "ui/assets/logo.h"
#include "ui/theme.h"

namespace ThetaGP::Ui::Splash {

using Theme::colour;

namespace {

struct Wordmark {
  lv_obj_t *cover = NULL;
  lv_obj_t *logo = NULL;
  bool done = false;
};

Wordmark s_wordmark;

void setOpa(void *var, int32_t value) {
  Wordmark *mark = (Wordmark *)var;
  const lv_opa_t opa = (lv_opa_t)value;
  lv_obj_set_style_bg_opa(mark->cover, opa, 0);
  lv_obj_set_style_image_opa(mark->logo, opa, 0);
}

void gone(lv_anim_t *anim) {
  s_wordmark.done = true;
  (void)anim;
  if (s_wordmark.cover != NULL) {
    lv_obj_delete(s_wordmark.cover);
  }
  s_wordmark.cover = NULL;
  s_wordmark.logo = NULL;
}

} // namespace

void show(lv_obj_t *root) {
  lv_obj_t *cover = lv_obj_create(root);
  lv_obj_remove_style_all(cover);
  lv_obj_set_pos(cover, 0, 0);
  lv_obj_set_size(cover, 420, 140);
  lv_obj_set_style_bg_color(cover, colour(Theme::BG), 0);
  lv_obj_remove_flag(cover, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(cover, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *logo = lv_image_create(cover);
  lv_image_set_src(logo, &thetagp_logo);
  lv_obj_set_style_image_recolor(logo, lv_color_hex(0x7EB4F2), 0);
  lv_obj_set_style_image_recolor_opa(logo, LV_OPA_COVER, 0);
  lv_obj_align(logo, LV_ALIGN_CENTER, 0, 0);

  s_wordmark.cover = cover;
  s_wordmark.logo = logo;
  setOpa(&s_wordmark, LV_OPA_TRANSP);

  lv_anim_t in;
  lv_anim_init(&in);
  lv_anim_set_var(&in, &s_wordmark);
  lv_anim_set_values(&in, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_exec_cb(&in, setOpa);
  lv_anim_set_path_cb(&in, lv_anim_path_ease_in_out);
  lv_anim_set_duration(&in, 400);
  lv_anim_start(&in);

  lv_anim_t out;
  lv_anim_init(&out);
  lv_anim_set_var(&out, &s_wordmark);
  lv_anim_set_values(&out, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_exec_cb(&out, setOpa);
  lv_anim_set_path_cb(&out, lv_anim_path_ease_in_out);
  lv_anim_set_delay(&out, 1800);
  lv_anim_set_duration(&out, 550);
  lv_anim_set_completed_cb(&out, gone);
  lv_anim_start(&out);
}

void drop(void) {
  if (s_wordmark.cover == NULL) {
    return;
  }
  s_wordmark.done = true;
  lv_anim_delete(&s_wordmark, NULL);
  s_wordmark.cover = NULL;
  s_wordmark.logo = NULL;
}

bool done(void) { return s_wordmark.done; }

} // namespace ThetaGP::Ui::Splash
