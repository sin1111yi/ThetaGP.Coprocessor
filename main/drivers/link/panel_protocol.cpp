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

#include "drivers/link/panel_protocol.h"

#include <cstdio>
#include <cstring>

#include "esp_timer.h"

#include "build_info.h"
#include "conf/board.h"
#include "drivers/display/nv3007_panel.h"
#include "ui/menu.h"

#include "Link.pb.h"
#include "pb_decode.h"
#include "pb_encode.h"

namespace ThetaGP::Drivers::Link {

namespace {

// The schema's own version, as the greetings carry it.
constexpr uint32_t kProtocol = 1;
// The most one message takes on the wire, with room for the tags nanopb writes.
constexpr size_t kPayloadMax = 96;
// How often the panel says what it shows, and how often it greets a board that
// has not answered yet. A greeting after an answer is only a keepalive.
constexpr uint32_t kBeatMs = 500;
constexpr uint32_t kHelloMs = 1000;
constexpr uint32_t kHelloIdleMs = 10000;

uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
uint32_t nowSeconds() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000000);
}

// The page ids of Link.proto and the ones the menu knows are the same numbers:
// the beat the panel sends and the state a host reads name a page the same way.
static_assert((uint8_t)Ui::Menu::PageId::Menu == ThetaGP_Link_Page_PAGE_MENU,
              "the menu's first page is not the schema's");
static_assert((uint8_t)Ui::Menu::PageId::Monitor ==
                  ThetaGP_Link_Page_PAGE_MONITOR,
              "the monitor page is not the schema's");
static_assert((uint8_t)Ui::Menu::PageId::Options ==
                  ThetaGP_Link_Page_PAGE_OPTIONS,
              "the options page is not the schema's");
static_assert((uint8_t)Ui::Menu::PageId::Mapping ==
                  ThetaGP_Link_Page_PAGE_MAPPING,
              "the mapping page is not the schema's");
static_assert((uint8_t)Ui::Menu::PageId::Profiles ==
                  ThetaGP_Link_Page_PAGE_PROFILES,
              "the profiles page is not the schema's");
static_assert((uint8_t)Ui::Menu::PageId::About == ThetaGP_Link_Page_PAGE_ABOUT,
              "the about page is not the schema's");

// One message, in the frame the transport writes.
bool send(const ThetaGP_Link_Message &message) {
  uint8_t payload[kPayloadMax] = {};
  pb_ostream_t stream = pb_ostream_from_buffer(payload, sizeof payload);
  if (!pb_encode(&stream, ThetaGP_Link_Message_fields, &message)) {
    printf("link: encode refused: %s\n", PB_GET_ERROR(&stream));
    return false;
  }
  return Link::getInstance().send(payload,
                                  static_cast<uint16_t>(stream.bytes_written));
}

// A fixed-width field out of a decoded message, which arrives as a C string.
void copyField(char *out, size_t size, const char *field) {
  std::strncpy(out, field, size - 1);
  out[size - 1] = '\0';
}

} // namespace

void PanelProtocol::init() {
  Link::getInstance().setHandler(onFrame, this);
  sendHello();
  _greeted = true;
  _lastHelloMs = nowMs();
  beat(true);
}

void PanelProtocol::tick() {
  apply();

  const uint32_t now = nowMs();
  const bool heard = Link::getInstance().heard();

  // A board that has answered nothing is greeted once a second; one that has
  // is greeted every ten, so a board that reboots is found again.
  const uint32_t hello = heard ? kHelloIdleMs : kHelloMs;
  if (now - _lastHelloMs >= hello) {
    _lastHelloMs = now;
    sendHello();
  }

  if (now - _lastBeatMs >= kBeatMs || shape() != _lastShape) {
    beat(true);
  }
}

void PanelProtocol::sendHello() {
  ThetaGP_Link_Message message = ThetaGP_Link_Message_init_zero;
  message.which_kind = ThetaGP_Link_Message_hello_tag;
  message.kind.hello.protocol = kProtocol;
  copyField(message.kind.hello.panel_id, sizeof message.kind.hello.panel_id,
            BuildInfo::NAME);
  copyField(message.kind.hello.release, sizeof message.kind.hello.release,
            BuildInfo::VERSION);
  // The surface as it is mounted: the panel is on its side, so the wider
  // number is the width the menu draws.
  message.kind.hello.width = LCD_V_RES;
  message.kind.hello.height = LCD_H_RES;
  (void)send(message);
}

uint32_t PanelProtocol::shape() const {
  const Ui::Menu::Screen screen = Ui::Menu::screen();
  uint32_t shape = (uint32_t)screen.page;
  shape = shape << 8 | screen.item;
  shape = shape << 1 | (Link::getInstance().heard() ? 1u : 0u);
  shape = shape << 1 | (Ui::Menu::introDone() ? 1u : 0u);
  shape = shape << 1 |
          (Display::Nv3007Panel::getInstance().backlightOn() ? 1u : 0u);
  return shape;
}

void PanelProtocol::beat(bool now) {
  (void)now;
  const Ui::Menu::Screen screen = Ui::Menu::screen();

  ThetaGP_Link_Message message = ThetaGP_Link_Message_init_zero;
  message.which_kind = ThetaGP_Link_Message_beat_tag;
  message.kind.beat.uptime_s = nowSeconds();
  message.kind.beat.page = (uint32_t)screen.page;
  message.kind.beat.item = screen.item;
  message.kind.beat.link = Link::getInstance().heard();
  message.kind.beat.backlight =
      Display::Nv3007Panel::getInstance().backlightOn();
  message.kind.beat.intro_done = Ui::Menu::introDone();
  message.kind.beat.dropped = Link::getInstance().refused();

  if (send(message)) {
    _lastBeatMs = nowMs();
    _lastShape = shape();
  }
}

void PanelProtocol::onFrame(const uint8_t *payload, uint16_t length,
                            void *context) {
  static_cast<PanelProtocol *>(context)->handle(payload, length);
}

void PanelProtocol::handle(const uint8_t *payload, uint16_t length) {
  ThetaGP_Link_Message message = ThetaGP_Link_Message_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(payload, length);
  if (!pb_decode(&stream, ThetaGP_Link_Message_fields, &message)) {
    // Not one of ours: a frame from a build that speaks another schema, or
    // noise the checksum let through. The transport counts it; nothing here
    // has a use for it.
    return;
  }

  switch (message.which_kind) {
  case ThetaGP_Link_Message_board_tag:
    copyField(_boardId, sizeof _boardId, message.kind.board.board_id);
    copyField(_boardRelease, sizeof _boardRelease, message.kind.board.release);
    _boardPending = true;
    break;

  case ThetaGP_Link_Message_state_tag:
    _stateHz = message.kind.state.report_hz;
    _stateId = message.kind.state.profile_id;
    _stateCount = message.kind.state.profile_count;
    copyField(_stateName, sizeof _stateName, message.kind.state.profile_name);
    _statePending = true;
    break;

  case ThetaGP_Link_Message_nav_tag: {
    const uint8_t next = (uint8_t)((_navHead + 1) % kNavQueue);
    if (next == _navTail) {
      // The queue is full: the step is dropped rather than overwriting the one
      // that is still to be applied.
      return;
    }
    _navAction[_navHead] = (uint8_t)message.kind.nav.action;
    _navHead = next;
    break;
  }

  default:
    break;
  }
}

void PanelProtocol::apply() {
  if (_boardPending) {
    _boardPending = false;
    // The board is named again every couple of seconds, and only a name that
    // changed is worth a line.
    if (std::strcmp(_boardId, Ui::model().peer_id) != 0 ||
        std::strcmp(_boardRelease, Ui::model().peer_version) != 0) {
      printf("link: board %s %s\n", _boardId, _boardRelease);
    }
    Ui::Menu::setPeer(_boardId, _boardRelease);
  }

  if (_statePending) {
    _statePending = false;
    Ui::Menu::setReportHz(_stateHz);
    Ui::Menu::setProfile(_stateId, _stateCount, _stateName);
  }

  bool moved = false;
  while (_navTail != _navHead) {
    const uint8_t action = _navAction[_navTail];
    _navTail = (uint8_t)((_navTail + 1) % kNavQueue);

    // The six keys, as the menu knows them: the four directions travel the
    // column, and the two ends of a choice take and refuse it.
    switch (action) {
    case ThetaGP_Link_NavAction_NAV_UP:
    case ThetaGP_Link_NavAction_NAV_LEFT:
      Ui::Menu::prev();
      break;
    case ThetaGP_Link_NavAction_NAV_DOWN:
    case ThetaGP_Link_NavAction_NAV_RIGHT:
      Ui::Menu::next();
      break;
    case ThetaGP_Link_NavAction_NAV_CONFIRM:
      Ui::Menu::enter();
      break;
    case ThetaGP_Link_NavAction_NAV_BACK:
      Ui::Menu::back();
      break;
    default:
      continue;
    }
    moved = true;
  }

  // A step is on screen now: one beat leaves at once, so the board reads what
  // it did instead of waiting for the next one.
  if (moved) {
    beat(true);
  }
}

} // namespace ThetaGP::Drivers::Link
