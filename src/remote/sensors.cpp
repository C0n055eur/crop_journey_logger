#include "sensors.h"
#include "pins.h"

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Wire.h>
#include <math.h>

namespace {

DHT dht(PIN_DHT, DHT_MODEL);
Adafruit_MPU6050 mpu;
bool g_imuReady = false;

SensorReading g_reading;
uint32_t g_lastDhtMs = 0;
uint32_t g_lastImuMs = 0;

const uint32_t kDhtIntervalMs = 2000;  // DHT22 hardware minimum
const uint32_t kImuIntervalMs = 20;    // 50 Hz

const float kGravity = 9.80665f;

}  // namespace

bool sensorsBegin() {
  dht.begin();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  g_imuReady = mpu.begin();
  if (g_imuReady) {
    // 8 g so a real jolt does not clip; the 2 g default saturates at the very
    // shocks we are trying to measure.
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
  }
  return g_imuReady;
}

void sensorsPoll() {
  const uint32_t now = millis();

  if (now - g_lastDhtMs >= kDhtIntervalMs) {
    g_lastDhtMs = now;
    const float t = dht.readTemperature();
    const float h = dht.readHumidity();
    if (!isnan(t)) g_reading.tempC = t;
    if (!isnan(h)) g_reading.humPct = h;
  }

  if (g_imuReady && now - g_lastImuMs >= kImuIntervalMs) {
    g_lastImuMs = now;
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    const float ax = a.acceleration.x, ay = a.acceleration.y, az = a.acceleration.z;
    const float mag = sqrtf(ax * ax + ay * ay + az * az);

    // Standing still the box reads 1 g. What damages produce is the departure
    // from that, in either direction.
    g_reading.shockNowG = fabsf(mag / kGravity - 1.0f);
    if (g_reading.shockNowG > g_reading.shockG) g_reading.shockG = g_reading.shockNowG;

    if (mag > 0.01f) {
      float c = az / mag;
      if (c > 1.0f) c = 1.0f;
      if (c < -1.0f) c = -1.0f;
      g_reading.tiltDeg = acosf(c) * RAD_TO_DEG;
    }
  }
}

SensorReading sensorsRead() { return g_reading; }

void sensorsResetPeak() { g_reading.shockG = 0.0f; }

bool sensorsImuReady() { return g_imuReady; }
