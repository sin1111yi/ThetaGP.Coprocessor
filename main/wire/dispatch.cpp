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

#include "wire/dispatch.h"

#include <cstring>

#include "Link.pb.h"
#include "pb_decode.h"

namespace ThetaGP::Wire {

namespace {

// A fixed-width field out of a decoded message, which arrives as a C string.
void copyField(char *out, size_t size, const char *field) {
  std::strncpy(out, field, size - 1);
  out[size - 1] = '\0';
}

// The board has said which one it is.
void takeBoard(const ThetaGP_Link_Message &message, Inbox &inbox) {
  copyField(inbox.boardId, sizeof inbox.boardId, message.kind.board.board_id);
  copyField(inbox.release, sizeof inbox.release, message.kind.board.release);
  inbox.boardPending = true;
}

// The board has said what a profile and the report rate now are.
void takeState(const ThetaGP_Link_Message &message, Inbox &inbox) {
  inbox.hz = message.kind.state.report_hz;
  inbox.profileId = message.kind.state.profile_id;
  inbox.profileCount = message.kind.state.profile_count;
  copyField(inbox.profileName, sizeof inbox.profileName,
            message.kind.state.profile_name);
  inbox.statePending = true;
}

// One step for the panel, behind the ones already waiting.
void takeNav(const ThetaGP_Link_Message &message, Inbox &inbox) {
  if (inbox.navCount == Inbox::kNavMax) {
    // The inbox is full: the step is dropped rather than overwriting the one
    // that is still to be applied.
    return;
  }
  inbox.nav[inbox.navCount] = static_cast<uint8_t>(message.kind.nav.action);
  ++inbox.navCount;
}

} // namespace

bool Dispatch::take(const uint8_t *payload, uint16_t length, Inbox &inbox) {
  ThetaGP_Link_Message message = ThetaGP_Link_Message_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(payload, length);
  if (!pb_decode(&stream, ThetaGP_Link_Message_fields, &message)) {
    return false;
  }

  switch (message.which_kind) {
  case ThetaGP_Link_Message_board_tag:
    takeBoard(message, inbox);
    break;
  case ThetaGP_Link_Message_state_tag:
    takeState(message, inbox);
    break;
  case ThetaGP_Link_Message_nav_tag:
    takeNav(message, inbox);
    break;
  default:
    // An arm this build has no use for: the frame was the schema's, so it is
    // taken as read and nothing is done with it.
    break;
  }
  return true;
}

} // namespace ThetaGP::Wire
