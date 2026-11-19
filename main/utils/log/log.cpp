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

#include "utils/log/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ThetaGP::Log {

namespace {

// One line, prefix and all, before it leaves. The prefix is `[LEVEL][file:line]:`,
// which is what the ring does not keep.
constexpr size_t kLine = 256;

// The lines the logger keeps, written on the slow path and read only when a
// task or a host asks.
struct Entry {
  uint32_t seq;
  Level level;
  char text[kLineText];
};

Entry s_ring[kRingLines];
uint32_t s_written;

Level s_level = Level::Info;
Sink s_sink;

// Two tasks write lines (the link's and the one that owns the widgets), so the
// ring and the sink are taken one line at a time. Null before `init`, which is
// where a line first needs it.
SemaphoreHandle_t s_mutex;

const char *hint(Level at) {
  switch (at) {
  case Level::Info:
    return "INFO ";
  case Level::Warn:
    return "WARN ";
  case Level::Error:
    return "ERROR";
  case Level::Debug:
    return "DEBUG";
  }
  return "     ";
}

// The last directory of the path the compiler saw, so a line names its file
// without the whole build path.
const char *shortPath(const char *file) {
  const char *slash = std::strrchr(file, '/');
  return slash != nullptr ? slash + 1 : file;
}

void keep(Level at, const char *text) {
  Entry &entry = s_ring[s_written % kRingLines];
  entry.seq = ++s_written;
  entry.level = at;
  std::strncpy(entry.text, text, kLineText - 1);
  entry.text[kLineText - 1] = '\0';
}

void toConsole(const char *line, size_t length) {
  std::fwrite(line, 1, length, stdout);
}

} // namespace

void init(Sink sink, Level at) {
  s_sink = sink != nullptr ? sink : toConsole;
  s_level = at;
  if (s_mutex == nullptr) {
    s_mutex = xSemaphoreCreateMutex();
  }
}

void setLevel(Level at) { s_level = at; }

Level level(void) { return s_level; }

void write(Level at, const char *file, int line, const char *format, ...) {
  // A line below the threshold is dropped before anything is formatted.
  if (at != Level::Debug && at < s_level) {
    return;
  }

  if (s_mutex != nullptr) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
  }

  char buffer[kLine] = {};
  const int prefix =
      std::snprintf(buffer, sizeof buffer, "[%s][%s:%d]: ", hint(at),
                    shortPath(file), line);
  int used = prefix > 0 && (size_t)prefix < sizeof buffer ? prefix : 0;
  if (used != 0) {
    va_list args;
    va_start(args, format);
    vsnprintf(buffer + used, sizeof buffer - (size_t)used, format, args);
    va_end(args);
  }

  // The line is kept without the prefix: a reader that has the level has no use
  // for it repeated, and the room is better spent on the message.
  keep(at, buffer + used);

  size_t length = std::strlen(buffer);
  if (length + 1 < sizeof buffer) {
    buffer[length++] = '\n';
    buffer[length] = '\0';
  }
  if (s_sink != nullptr) {
    s_sink(buffer, length);
  }

  if (s_mutex != nullptr) {
    xSemaphoreGive(s_mutex);
  }
}

uint32_t total(void) { return s_written; }

uint8_t read(uint32_t since, LineView *out, uint8_t max) {
  if (out == nullptr || max == 0) {
    return 0;
  }
  const uint32_t oldest =
      s_written > kRingLines ? s_written - kRingLines + 1 : 1;
  uint32_t from = since + 1;
  if (from < oldest) {
    from = oldest; // the lines before this one are gone
  }

  uint8_t count = 0;
  for (uint32_t seq = from; seq <= s_written && count < max; ++seq) {
    const Entry &entry = s_ring[(seq - 1) % kRingLines];
    out[count].seq = entry.seq;
    out[count].level = entry.level;
    out[count].text = entry.text;
    ++count;
  }
  return count;
}

} // namespace ThetaGP::Log
