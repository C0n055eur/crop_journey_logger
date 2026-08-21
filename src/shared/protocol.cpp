#include "protocol.h"

bool cjlParse(const char *line, CjlFrame &out) {
  if (!line) return false;

  // Skip leading whitespace, tolerate a trailing CR from a terminal.
  while (*line == ' ' || *line == '\t') line++;
  strncpy(out.raw, line, CJL_FRAME_MAX - 1);
  out.raw[CJL_FRAME_MAX - 1] = '\0';
  for (char *p = out.raw; *p; p++) {
    if (*p == '\r' || *p == '\n') {
      *p = '\0';
      break;
    }
  }

  out.count = 0;
  char *cursor = out.raw;
  while (out.count < CJL_MAX_FIELDS) {
    out.field[out.count++] = cursor;
    char *comma = strchr(cursor, ',');
    if (!comma) break;
    *comma = '\0';
    cursor = comma + 1;
  }

  if (out.count < 4) return false;
  if (strcmp(out.field[0], CJL_MAGIC) != 0) return false;
  if (atoi(out.field[1]) != CJL_VERSION) return false;
  return true;
}
