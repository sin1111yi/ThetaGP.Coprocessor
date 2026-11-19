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

#include "ui/model.h"

#include <cstring>

#include "ui/menu.h"

namespace ThetaGP::Ui {

namespace {

Model s_model;

// The board the link reports is the board the pages name, so the About page
// says where the screen it is on actually is.
char s_boardId[24] = "ThetaGP-H743";

} // namespace

const Model &model() { return s_model; }

namespace Menu {

void setLink(bool up) {
  if (s_model.link == up) {
    return;
  }
  s_model.link = up;
  refresh();
}

void setReportHz(uint32_t hz) {
  if (hz == s_model.report_hz) {
    return;
  }
  s_model.report_hz = hz;
  refresh();
}

void setPeer(const char *id, const char *version) {
  setBoard(id);
  if (id != NULL) {
    std::strncpy(s_model.peer_id, id, sizeof s_model.peer_id - 1);
    s_model.peer_id[sizeof s_model.peer_id - 1] = '\0';
  }
  if (version != NULL) {
    std::strncpy(s_model.peer_version, version,
                 sizeof s_model.peer_version - 1);
    s_model.peer_version[sizeof s_model.peer_version - 1] = '\0';
  }
}

void setProfile(uint32_t id, uint32_t count, const char *name) {
  const bool same =
      id == s_model.profile_id && count == s_model.profile_count &&
      (name == NULL || std::strcmp(name, s_model.profile_name) == 0);
  if (same) {
    return;
  }
  s_model.profile_id = id;
  s_model.profile_count = count;
  if (name != NULL) {
    s_model.profile_name = name;
  }
  refresh();
}

void setBoard(const char *id) {
  if (id == NULL || id[0] == '\0') {
    return;
  }
  std::strncpy(s_boardId, id, sizeof s_boardId - 1);
  s_boardId[sizeof s_boardId - 1] = '\0';
  s_model.board = s_boardId;
}

} // namespace Menu

} // namespace ThetaGP::Ui
