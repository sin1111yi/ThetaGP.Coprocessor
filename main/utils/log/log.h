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

// One place a line leaves the board from: a level, the file and line it was
// written at, and the message. The last lines are kept in a ring, so a host
// that reads the port late still sees what the firmware said, and a line below
// the threshold is dropped before it is formatted.
//
// The same shape as the main board's `utils/log`: levels, a compile-time switch
// for debug lines, a runtime threshold, and a ring of the last lines.

#pragma once

#include <cstddef>
#include <cstdint>

// Debug lines are compiled in only by a build that asks for them: the rest
// carry neither the format nor the ring entry.
#ifndef THETAGP_LOG_DEBUG
#define THETAGP_LOG_DEBUG 0
#endif

namespace ThetaGP::Log {

// The order is the threshold's: a line below the level that is set is dropped.
enum class Level : uint8_t { Info = 0, Warn = 1, Error = 2, Debug = 3 };

// Where a formatted line goes: the console unless the board hands in another.
using Sink = void (*)(const char *line, size_t length);

// Claims the sink and sets the threshold. Called once, from the application.
void init(Sink sink = nullptr, Level at = Level::Info);

// The threshold lines are written at; a profile may move it later.
void setLevel(Level at);
Level level(void);

// Writes one line: `[LEVEL][file:line]: message`. The message alone is what the
// ring keeps.
void write(Level at, const char *file, int line, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

// ── the lines already written ──

inline constexpr uint8_t kRingLines = 16;
inline constexpr size_t kLineText = 96;

// One line, as a reader is handed it: its number from boot, its level, and the
// text without the prefix.
struct LineView {
  uint32_t seq;
  Level level;
  const char *text;
};

// Lines written since boot: a reader asks from the number it holds up to this.
uint32_t total(void);

// Up to `max` lines after `since`, oldest first. A reader that fell behind the
// ring is handed what is still there.
uint8_t read(uint32_t since, LineView *out, uint8_t max);

} // namespace ThetaGP::Log

#define LOG_INFO(...)                                                          \
  ThetaGP::Log::write(ThetaGP::Log::Level::Info, __FILE__, __LINE__,           \
                      __VA_ARGS__)
#define LOG_WARN(...)                                                          \
  ThetaGP::Log::write(ThetaGP::Log::Level::Warn, __FILE__, __LINE__,           \
                      __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
  ThetaGP::Log::write(ThetaGP::Log::Level::Error, __FILE__, __LINE__,          \
                      __VA_ARGS__)
#if THETAGP_LOG_DEBUG
#define LOG_DEBUG(...)                                                         \
  ThetaGP::Log::write(ThetaGP::Log::Level::Debug, __FILE__, __LINE__,          \
                      __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#endif
