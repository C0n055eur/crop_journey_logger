#pragma once
#include <Arduino.h>

// Everything the farm node knows about the consignment in flight, rebuilt from
// the frames it has received. Deliberately a plain struct: the base node is a
// dumb display, all the judgement happens in the truck.

#define BASE_ALERT_HISTORY 8
#define BASE_CUSTODY_MAX 6

struct BaseAlert {
  char metric[10];
  float value;
  float limit;
  double lat;
  double lon;
  uint32_t journeyS;
};

struct BaseCustody {
  char role[16];
  uint32_t journeyS;
};

struct BaseModel {
  bool journeyActive = false;
  char crop[12] = "-";
  float tempLimit = 0, humMin = 0, humMax = 0, shockLimit = 0;

  uint32_t packets = 0;
  uint32_t lastRxMs = 0;

  // Latest heartbeat.
  bool haveLive = false;
  float temp = NAN, hum = NAN, shock = 0;
  char status[8] = "-";
  double lat = 0, lon = 0;
  uint32_t journeyS = 0;

  // Alerts, newest first.
  BaseAlert alerts[BASE_ALERT_HISTORY];
  int alertCount = 0;

  BaseCustody custody[BASE_CUSTODY_MAX];
  int custodyCount = 0;

  // End-of-journey summary.
  bool haveSummary = false;
  float sTempMax = 0, sTempMin = 0, sHumMin = 0, sHumMax = 0, sShockMax = 0;
  uint16_t sExcursions = 0;
  uint32_t sDurationS = 0;
  char sGrade[12] = "-";
};
