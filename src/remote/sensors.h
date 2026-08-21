#pragma once
#include <Arduino.h>

struct SensorReading {
  float tempC = NAN;
  float humPct = NAN;
  float shockG = 0.0f;   // peak deviation from 1 g since the last reset
  float shockNowG = 0.0f;  // instantaneous, for the LED
  float tiltDeg = 0.0f;
};

bool sensorsBegin();

// Call every loop. Samples the IMU at ~50 Hz so a pothole between two log rows
// is still caught, and re-reads the DHT at its 2 s minimum interval.
void sensorsPoll();

SensorReading sensorsRead();

// Clear the shock peak-hold; called after each log row is written.
void sensorsResetPeak();

bool sensorsImuReady();
