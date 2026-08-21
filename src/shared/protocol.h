#pragma once
#include <Arduino.h>

// Wire format for the farm link.
//
// Plain comma-separated ASCII, under 120 bytes, so it fits a single LoRa
// payload at SF12 and stays readable if anyone ever sniffs the air or opens the
// serial log. Both nodes build and parse frames through this one header, so the
// two ends cannot drift apart.
//
//   CJL,1,<seq>,A,<crop>,<metric>,<value>,<limit>,<lat>,<lon>,<journey_s>
//   CJL,1,<seq>,C,<role>,<uid>,<lat>,<lon>,<journey_s>
//   CJL,1,<seq>,H,<crop>,<temp>,<hum>,<shock>,<status>,<lat>,<lon>,<journey_s>
//   CJL,1,<seq>,S,<crop>,<tmax>,<tmin>,<hmin>,<hmax>,<shockmax>,<nexc>,<dur_s>,<grade>
//   CJL,1,<seq>,J,<crop>,<tempmax>,<hummin>,<hummax>,<shockmax>
//
// Field 0 magic, 1 version, 2 sequence, 3 type. Everything after that is
// type-specific.

#define CJL_MAGIC "CJL"
#define CJL_VERSION 1
#define CJL_FRAME_MAX 160
#define CJL_MAX_FIELDS 16

// Frame types.
#define CJL_ALERT 'A'
#define CJL_CUSTODY 'C'
#define CJL_HEARTBEAT 'H'
#define CJL_SUMMARY 'S'
#define CJL_JOURNEY_START 'J'

struct CjlFrame {
  char raw[CJL_FRAME_MAX];
  const char *field[CJL_MAX_FIELDS];
  int count = 0;

  char type() const { return count > 3 ? field[3][0] : '?'; }
  uint32_t seq() const { return count > 2 ? strtoul(field[2], nullptr, 10) : 0; }
  const char *str(int i) const { return (i >= 0 && i < count) ? field[i] : ""; }
  float num(int i) const { return (i >= 0 && i < count) ? atof(field[i]) : 0.0f; }
  long integer(int i) const { return (i >= 0 && i < count) ? atol(field[i]) : 0; }
};

// Tokenises `line` into `out`. Returns false unless it is a well-formed frame
// of a version we understand.
bool cjlParse(const char *line, CjlFrame &out);
