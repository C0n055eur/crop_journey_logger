#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Journey time
// ---------------------------------------------------------------------------
// A real trip is 4-10 hours; a judge has two minutes. TIME_SCALE compresses
// wall-clock into journey-clock so the CSV still carries honest 2-minute
// timestamps across a 6-hour trip while the demo finishes in ~6 minutes.
// Set TIME_SCALE to 1 for field hardware (see platformio.ini).
#ifndef TIME_SCALE
#define TIME_SCALE 1.0f
#endif

// One log row every two minutes of journey time. This is the number the
// problem statement fixes, and it does not change with TIME_SCALE.
static const uint32_t LOG_INTERVAL_JOURNEY_S = 120;

// A nominal 4-hour farm-to-mandi run: Kolar to Yeshwanthpur APMC is about 65 km
// of state highway, and a loaded truck with a collection-centre halt takes that
// long. Both figures sit inside the 50-200 km / 4-10 h envelope in the problem
// statement.
static const uint32_t JOURNEY_TOTAL_S = 4UL * 3600UL;

// ---------------------------------------------------------------------------
// End-of-journey record
// ---------------------------------------------------------------------------
struct JourneyStats {
  float tempMax = -999.0f;
  float tempMin = 999.0f;
  float humMax = -999.0f;
  float humMin = 999.0f;
  float shockMax = 0.0f;
  float shockMaxLat = 0.0f;  // where the worst jolt happened
  float shockMaxLon = 0.0f;
  uint32_t shockMaxAtS = 0;
  uint16_t tempExcursions = 0;
  uint16_t humExcursions = 0;
  uint16_t shockExcursions = 0;
  uint32_t rows = 0;
  uint32_t durationS = 0;

  uint16_t totalExcursions() const {
    return tempExcursions + humExcursions + shockExcursions;
  }
};

// A one-word verdict the farmer can wave at the trader.
inline const char *journeyGrade(const JourneyStats &s) {
  const uint16_t n = s.totalExcursions();
  if (n == 0) return "A-CLEAN";
  if (n <= 2) return "B-MINOR";
  if (n <= 5) return "C-POOR";
  return "D-BAD";
}

// ---------------------------------------------------------------------------
// Chain of custody
// ---------------------------------------------------------------------------
static const char *const CUSTODY_ROLES[] = {"FARM", "COLLECTION", "MANDI"};
static const int CUSTODY_ROLE_COUNT = 3;
