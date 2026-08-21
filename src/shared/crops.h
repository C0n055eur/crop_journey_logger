#pragma once
#include <Arduino.h>

// Per-crop handling limits for the farm-to-mandi leg.
//
// Temperature and humidity bands are the usual short-haul transport
// recommendations for Karnataka produce; shockG is the acceleration deviation
// (in g, away from the 1 g the box feels standing still) at which bruising
// starts to matter for that crop. Soft fruit gets a low number, tubers a high
// one.
struct CropPreset {
  const char *name;
  float tempMax;  // deg C
  float tempMin;  // deg C  (chilling injury below this)
  float humMin;   // %RH
  float humMax;   // %RH
  float shockG;   // g deviation
  const char *note;
};

extern const CropPreset CROPS[];
extern const int CROP_COUNT;

// Which limits the current reading breaks. Bit flags so the LED can simply
// count them and the alert path knows which metric to name.
enum BreachFlags : uint8_t {
  BREACH_NONE = 0,
  BREACH_TEMP = 1 << 0,
  BREACH_HUM = 1 << 1,
  BREACH_SHOCK = 1 << 2,
};

uint8_t cropEvaluate(const CropPreset &crop, float tempC, float humPct, float shockG);
int breachCount(uint8_t flags);
