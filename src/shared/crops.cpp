#include "crops.h"

const CropPreset CROPS[] = {
    // name        tmax  tmin  hmin  hmax  shock  note
    {"TOMATO",     25.0f, 10.0f, 85.0f, 95.0f, 2.0f, "bruises easily, chills below 10C"},
    {"ONION",      30.0f,  0.0f, 60.0f, 75.0f, 3.0f, "needs dry air, cracks when dropped"},
    {"BANANA",     28.0f, 13.0f, 85.0f, 95.0f, 1.5f, "very soft, chills below 13C"},
    {"GRAPES",     22.0f, -1.0f, 85.0f, 95.0f, 1.8f, "shatters off the bunch"},
    {"POTATO",     28.0f,  7.0f, 85.0f, 95.0f, 3.0f, "hardy, greens in the heat"},
    {"CHILLI",     27.0f,  7.0f, 90.0f, 95.0f, 2.0f, "wilts fast in dry heat"},
};

const int CROP_COUNT = sizeof(CROPS) / sizeof(CROPS[0]);

uint8_t cropEvaluate(const CropPreset &crop, float tempC, float humPct, float shockG) {
  uint8_t flags = BREACH_NONE;
  if (!isnan(tempC) && (tempC > crop.tempMax || tempC < crop.tempMin)) flags |= BREACH_TEMP;
  if (!isnan(humPct) && (humPct > crop.humMax || humPct < crop.humMin)) flags |= BREACH_HUM;
  if (shockG > crop.shockG) flags |= BREACH_SHOCK;
  return flags;
}

int breachCount(uint8_t flags) {
  int n = 0;
  for (uint8_t b = 1; b; b <<= 1) {
    if (flags & b) n++;
  }
  return n;
}
