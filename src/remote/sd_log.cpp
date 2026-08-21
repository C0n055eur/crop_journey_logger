#include "sd_log.h"

#include <SD.h>
#include <SPI.h>

namespace {
bool g_ready = false;

const char *kHeader =
    "seq,journey_s,utc_time,lat,lon,speed_kmph,sats,temp_c,hum_pct,shock_g,tilt_deg,status,"
    "custodian,event";
}  // namespace

bool sdLogBegin(uint8_t csPin) {
  g_ready = SD.begin(csPin);
  if (!g_ready) {
    Serial.println("SD: card init FAILED - logging to serial only");
  } else {
    Serial.println("SD: card ready");
  }
  return g_ready;
}

bool sdLogAvailable() { return g_ready; }

bool sdLogStart() {
  if (!g_ready) return false;
  if (SD.exists(JOURNEY_CSV_PATH)) SD.remove(JOURNEY_CSV_PATH);
  File f = SD.open(JOURNEY_CSV_PATH, FILE_WRITE);
  if (!f) {
    Serial.println("SD: could not create " JOURNEY_CSV_PATH);
    g_ready = false;
    return false;
  }
  f.println(kHeader);
  f.close();
  return true;
}

void sdLogRow(const char *row) {
  if (!g_ready) return;
  File f = SD.open(JOURNEY_CSV_PATH, FILE_APPEND);
  if (!f) return;
  f.println(row);
  f.close();
}

void sdLogDumpToSerial() {
  Serial.println("---BEGIN JOURNEY.CSV---");
  if (!g_ready) {
    Serial.println("(no card)");
  } else {
    File f = SD.open(JOURNEY_CSV_PATH, FILE_READ);
    if (!f) {
      Serial.println("(file missing)");
    } else {
      while (f.available()) Serial.write(f.read());
      f.close();
    }
  }
  Serial.println("---END JOURNEY.CSV---");
}
