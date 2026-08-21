// Farm-to-Mandi Journey Logger - remote (in-truck) node.
//
// Powers on, lets the farmer pick the crop, waits for a GPS fix, then logs
// GPS + temperature + humidity + shock to a CSV on the microSD card every two
// minutes until power-off. Threshold breaches raise an immediate radio alert to
// the farm base node; RFID scans at each waypoint stamp the chain of custody;
// one button press prints the end-of-journey summary.

#include <Arduino.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

#include "pins.h"
#include "sensors.h"
#include "sd_log.h"
#include "custody.h"
#include "sim_gps.h"
#include "../shared/crops.h"
#include "../shared/journey.h"
#include "../shared/protocol.h"
#include "../shared/radio.h"

#ifndef SIM_BUILD
#define SIM_BUILD 1
#endif

// ---------------------------------------------------------------------------

enum State { STATE_SELECT, STATE_ACQUIRE, STATE_LOGGING, STATE_ENDED };

namespace {

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

State g_state = STATE_SELECT;
int g_crop = 0;
uint32_t g_seq = 0;

uint32_t g_journeyStartMs = 0;
uint32_t g_selectEnteredMs = 0;
uint32_t g_acquireStartMs = 0;
uint32_t g_nextLogS = 0;
uint32_t g_lastEvalMs = 0;
uint32_t g_lastShockMs = 0;
uint32_t g_firstFixMs = 0;  // millis() of the first valid fix, 0 until then

JourneyStats g_stats;
bool g_inBreach[3] = {false, false, false};  // temp, humidity, shock

// Event text attached to the next CSV row, e.g. "TEMP_EXCURSION".
char g_pendingEvent[48] = "";

// Crop selection auto-confirms so the box still "starts logging automatically
// when powered on" if nobody touches it.
const uint32_t kSelectTimeoutMs = 15000;
// Give up waiting for satellites rather than never logging at all.
const uint32_t kFixTimeoutMs = 120000;
// How long the red LED stays lit after a jolt, in wall-clock time, so a glance
// at the box still catches it.
const uint32_t kShockLatchMs = 3000;

// -------------------------------------------------------------- buttons

struct Button {
  explicit Button(uint8_t p) : pin(p) {}
  uint8_t pin;
  bool last = true;
  uint32_t changedMs = 0;
};

Button g_btnNext(PIN_BTN_NEXT);
Button g_btnOk(PIN_BTN_OK);

bool pressed(Button &b) {
  const bool level = digitalRead(b.pin);  // INPUT_PULLUP: LOW = pressed
  const uint32_t now = millis();
  if (level != b.last && now - b.changedMs > 40) {
    b.changedMs = now;
    b.last = level;
    return level == LOW;
  }
  return false;
}

// -------------------------------------------------------------- helpers

uint32_t journeySeconds() {
  if (g_state < STATE_LOGGING) return 0;
  return (uint32_t)((millis() - g_journeyStartMs) * TIME_SCALE / 1000.0f);
}

void utcString(char *out, size_t len) {
  if (gps.time.isValid() && gps.date.isValid()) {
    snprintf(out, len, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
  } else if (len) {
    out[0] = '\0';
  }
}

double curLat() { return gps.location.isValid() ? gps.location.lat() : 0.0; }
double curLon() { return gps.location.isValid() ? gps.location.lng() : 0.0; }

const char *statusName(int breaches) {
  if (breaches >= 2) return "ALERT";
  if (breaches == 1) return "WARN";
  return "OK";
}

void setLeds(int breaches, bool shockLatched) {
  const bool red = breaches >= 2 || shockLatched;
  const bool yellow = !red && breaches == 1;
  digitalWrite(PIN_LED_RED, red);
  digitalWrite(PIN_LED_YELLOW, yellow);
  digitalWrite(PIN_LED_GREEN, !red && !yellow);
}

void drainGps() {
  while (gpsSerial.available()) gps.encode((char)gpsSerial.read());

  // The constraint is measured from power-on, not from the moment we start
  // waiting, so latch it the first time a fix appears whatever state we are in.
  if (!g_firstFixMs && gps.location.isValid()) {
    g_firstFixMs = millis();
    Serial.printf("GPS: first fix %.1f s after power-on (constraint: within 120 s)\n",
                  g_firstFixMs / 1000.0f);
  }
}

// -------------------------------------------------------------- radio out

void sendJourneyStart() {
  const CropPreset &c = CROPS[g_crop];
  char f[CJL_FRAME_MAX];
  snprintf(f, sizeof(f), "%s,%d,%lu,%c,%s,%.1f,%.0f,%.0f,%.1f", CJL_MAGIC, CJL_VERSION,
           (unsigned long)++g_seq, CJL_JOURNEY_START, c.name, c.tempMax, c.humMin, c.humMax,
           c.shockG);
  radioSend(f);
}

void sendAlert(const char *metric, float value, float limit) {
  char f[CJL_FRAME_MAX];
  snprintf(f, sizeof(f), "%s,%d,%lu,%c,%s,%s,%.2f,%.2f,%.5f,%.5f,%lu", CJL_MAGIC, CJL_VERSION,
           (unsigned long)++g_seq, CJL_ALERT, CROPS[g_crop].name, metric, value, limit, curLat(),
           curLon(), (unsigned long)journeySeconds());
  radioSend(f);
}

void sendCustody(const char *role, const char *uid) {
  char f[CJL_FRAME_MAX];
  snprintf(f, sizeof(f), "%s,%d,%lu,%c,%s,%s,%.5f,%.5f,%lu", CJL_MAGIC, CJL_VERSION,
           (unsigned long)++g_seq, CJL_CUSTODY, role, uid, curLat(), curLon(),
           (unsigned long)journeySeconds());
  radioSend(f);
}

void sendHeartbeat(const SensorReading &r, const char *status) {
  char f[CJL_FRAME_MAX];
  snprintf(f, sizeof(f), "%s,%d,%lu,%c,%s,%.1f,%.1f,%.2f,%s,%.5f,%.5f,%lu", CJL_MAGIC, CJL_VERSION,
           (unsigned long)++g_seq, CJL_HEARTBEAT, CROPS[g_crop].name, r.tempC, r.humPct, r.shockG,
           status, curLat(), curLon(), (unsigned long)journeySeconds());
  radioSend(f);
}

void sendSummary() {
  char f[CJL_FRAME_MAX];
  snprintf(f, sizeof(f), "%s,%d,%lu,%c,%s,%.1f,%.1f,%.1f,%.1f,%.2f,%u,%lu,%s", CJL_MAGIC,
           CJL_VERSION, (unsigned long)++g_seq, CJL_SUMMARY, CROPS[g_crop].name, g_stats.tempMax,
           g_stats.tempMin, g_stats.humMin, g_stats.humMax, g_stats.shockMax,
           g_stats.totalExcursions(), (unsigned long)g_stats.durationS, journeyGrade(g_stats));
  radioSend(f);
}

// -------------------------------------------------------------- crop select

void printCrop() {
  const CropPreset &c = CROPS[g_crop];
  Serial.printf("CROP %d/%d: %-8s  temp %.0f-%.0f C | humidity %.0f-%.0f %% | shock %.1f g  (%s)\n",
                g_crop + 1, CROP_COUNT, c.name, c.tempMin, c.tempMax, c.humMin, c.humMax, c.shockG,
                c.note);
  Serial.println("  [NEXT] change crop   [OK] start journey");
}

// -------------------------------------------------------------- logging

void writeRow(const SensorReading &r, int breaches) {
  char utc[16];
  utcString(utc, sizeof(utc));

  char row[224];
  snprintf(row, sizeof(row),
           "%lu,%lu,%s,%.5f,%.5f,%.1f,%d,%.1f,%.1f,%.2f,%.1f,%s,%s,%s",
           (unsigned long)g_stats.rows, (unsigned long)journeySeconds(), utc, curLat(), curLon(),
           gps.speed.isValid() ? gps.speed.kmph() : 0.0, gps.satellites.isValid()
                                                             ? (int)gps.satellites.value()
                                                             : 0,
           r.tempC, r.humPct, r.shockG, r.tiltDeg, statusName(breaches), custodyCurrent(),
           g_pendingEvent);

  sdLogRow(row);
  Serial.print("LOG,");
  Serial.println(row);
  g_pendingEvent[0] = '\0';
}

void noteEvent(const char *event) {
  // Several events can land between two rows; keep them all on the row.
  if (g_pendingEvent[0]) {
    strncat(g_pendingEvent, "+", sizeof(g_pendingEvent) - strlen(g_pendingEvent) - 1);
    strncat(g_pendingEvent, event, sizeof(g_pendingEvent) - strlen(g_pendingEvent) - 1);
  } else {
    strncpy(g_pendingEvent, event, sizeof(g_pendingEvent) - 1);
    g_pendingEvent[sizeof(g_pendingEvent) - 1] = '\0';
  }
}

// Edge-triggered so a long hot stretch counts as one excursion, not hundreds.
// Recovery needs a margin so a value sitting exactly on the limit does not
// chatter.
// Returns true when a new excursion just started.
bool trackBreach(int index, bool breaching, bool recovered, const char *metric, float value,
                 float limit, uint16_t &counter, const char *eventName) {
  if (breaching && !g_inBreach[index]) {
    g_inBreach[index] = true;
    counter++;
    noteEvent(eventName);
    Serial.printf("!! %s EXCURSION: %.2f (limit %.2f) at %.5f,%.5f t+%lus\n", metric, value, limit,
                  curLat(), curLon(), (unsigned long)journeySeconds());
    sendAlert(metric, value, limit);
    return true;
  }
  if (g_inBreach[index] && recovered) {
    g_inBreach[index] = false;
    Serial.printf("   %s back within limits (%.2f)\n", metric, value);
  }
  return false;
}

void evaluate(const SensorReading &r) {
  const CropPreset &c = CROPS[g_crop];

  if (!isnan(r.tempC)) {
    g_stats.tempMax = max(g_stats.tempMax, r.tempC);
    g_stats.tempMin = min(g_stats.tempMin, r.tempC);
  }
  if (!isnan(r.humPct)) {
    g_stats.humMax = max(g_stats.humMax, r.humPct);
    g_stats.humMin = min(g_stats.humMin, r.humPct);
  }
  if (r.shockG > g_stats.shockMax) {
    g_stats.shockMax = r.shockG;
    g_stats.shockMaxLat = curLat();
    g_stats.shockMaxLon = curLon();
    g_stats.shockMaxAtS = journeySeconds();
  }

  const uint8_t flags = cropEvaluate(c, r.tempC, r.humPct, r.shockG);

  const bool tempHot = r.tempC > c.tempMax;
  bool started = trackBreach(0, (flags & BREACH_TEMP) != 0,
              !isnan(r.tempC) && r.tempC < c.tempMax - 0.5f && r.tempC > c.tempMin + 0.5f, "TEMP",
              r.tempC, tempHot ? c.tempMax : c.tempMin, g_stats.tempExcursions, "TEMP_EXCURSION");

  const bool humDry = r.humPct < c.humMin;
  started |= trackBreach(1, (flags & BREACH_HUM) != 0,
                         !isnan(r.humPct) && r.humPct < c.humMax - 2.0f &&
                             r.humPct > c.humMin + 2.0f,
                         "HUM", r.humPct, humDry ? c.humMin : c.humMax, g_stats.humExcursions,
                         "HUM_EXCURSION");

  started |= trackBreach(2, (flags & BREACH_SHOCK) != 0, r.shockG < c.shockG * 0.8f, "SHOCK",
                         r.shockG, c.shockG, g_stats.shockExcursions, "SHOCK_EVENT");

  if (r.shockNowG > c.shockG) g_lastShockMs = millis();

  // An excursion earns its own row. Waiting for the next two-minute slot would
  // lose the one thing the log exists to prove: exactly where it happened.
  if (started) {
    g_stats.rows++;
    writeRow(r, breachCount(flags));
  }

  setLeds(breachCount(flags), millis() - g_lastShockMs < kShockLatchMs);
}

void pollCustody() {
  char uid[24];
  const int index = custodyPoll(uid, sizeof(uid));
  if (index < 0) return;

  const uint32_t t = journeySeconds();
  custodyRecordStamp(index, t);
  const char *role = custodyRoleName(index);

  Serial.printf("CUSTODY: %s uid=%s at %.5f,%.5f t+%lus\n", role, uid, curLat(), curLon(),
                (unsigned long)t);

  char event[48];
  snprintf(event, sizeof(event), "CUSTODY_%s", role);
  noteEvent(event);
  sendCustody(role, uid);

  // Custody hand-overs are worth a row of their own, not just a note on the
  // next scheduled one: the timestamp is the whole point.
  const SensorReading r = sensorsRead();
  g_stats.rows++;
  writeRow(r, breachCount(cropEvaluate(CROPS[g_crop], r.tempC, r.humPct, r.shockG)));
}

// -------------------------------------------------------------- summary

void printSummary() {
  const CropPreset &c = CROPS[g_crop];
  g_stats.durationS = journeySeconds();

  Serial.println();
  Serial.println("========== JOURNEY SUMMARY ==========");
  Serial.printf("Crop            : %s (max %.0f C, %.0f-%.0f %%RH, %.1f g)\n", c.name, c.tempMax,
                c.humMin, c.humMax, c.shockG);
  Serial.printf("Duration        : %luh %02lum of journey time\n",
                (unsigned long)(g_stats.durationS / 3600),
                (unsigned long)((g_stats.durationS % 3600) / 60));
  Serial.printf("Rows logged     : %lu (every 2 min, plus one per event)\n",
                (unsigned long)g_stats.rows);
  Serial.printf("Max temperature : %.1f C  (limit %.1f C)  -> %u excursion(s)\n", g_stats.tempMax,
                c.tempMax, g_stats.tempExcursions);
  Serial.printf("Humidity range  : %.1f - %.1f %%RH  (band %.0f-%.0f)  -> %u excursion(s)\n",
                g_stats.humMin, g_stats.humMax, c.humMin, c.humMax, g_stats.humExcursions);
  Serial.printf("Max shock       : %.2f g  (limit %.2f g)  -> %u event(s)\n", g_stats.shockMax,
                c.shockG, g_stats.shockExcursions);
  Serial.printf("Worst jolt at   : %.5f,%.5f  t+%luh %02lum\n", g_stats.shockMaxLat,
                g_stats.shockMaxLon, (unsigned long)(g_stats.shockMaxAtS / 3600),
                (unsigned long)((g_stats.shockMaxAtS % 3600) / 60));
  Serial.printf("Total excursions: %u\n", g_stats.totalExcursions());

  Serial.print("Custody chain   : ");
  if (custodyCount() == 0) {
    Serial.print("(no scans)");
  } else {
    for (int i = 0; i < custodyCount(); i++) {
      if (i) Serial.print(" > ");
      const uint32_t t = custodyStampS(i);
      Serial.printf("%s %luh%02lum", custodyRoleName(i), (unsigned long)(t / 3600),
                    (unsigned long)((t % 3600) / 60));
    }
  }
  Serial.println();
  Serial.printf("VERDICT         : %s\n", journeyGrade(g_stats));
  Serial.println("=====================================");

  sendSummary();
  sdLogDumpToSerial();
}

}  // namespace

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== Farm-to-Mandi Journey Logger : REMOTE NODE ===");

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
  setLeds(0, false);

  // GPS. TX is unused - a NEO-6M only ever talks to us - and in simulation
  // GPIO17 belongs to the NMEA generator on UART2.
  gpsSerial.setRxBufferSize(1024);  // must precede begin(); the 128 B default overflows
  gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, -1);
  Serial.printf("GPS: UART1 RX=GPIO%d @9600\n", PIN_GPS_RX);
#if SIM_BUILD
  simGpsSetTimeScale(TIME_SCALE);
  simGpsBegin(PIN_GPS_SIM_TX);
  Serial.printf("GPS: simulated NEO-6M on GPIO%d, looped back to GPIO%d\n", PIN_GPS_SIM_TX,
                PIN_GPS_RX);
#endif

  Serial.printf("SENSORS: %s\n", sensorsBegin() ? "DHT22 + MPU6050 ready" : "MPU6050 NOT FOUND");
  sdLogBegin(PIN_SD_CS);
  custodyBegin(PIN_RFID_CS, PIN_RFID_RST);
  radioBegin();

  Serial.printf("CLOCK: journey time runs at %.0fx wall clock; a row every %lu journey-seconds\n",
                (float)TIME_SCALE, (unsigned long)LOG_INTERVAL_JOURNEY_S);
  Serial.println();
  Serial.println("--- SELECT CROP ---");
  printCrop();
  g_selectEnteredMs = millis();
}

void loop() {
  drainGps();
  sensorsPoll();

  switch (g_state) {
    case STATE_SELECT: {
      if (pressed(g_btnNext)) {
        g_crop = (g_crop + 1) % CROP_COUNT;
        printCrop();
        g_selectEnteredMs = millis();
      }
      const bool timedOut = millis() - g_selectEnteredMs > kSelectTimeoutMs;
      if (pressed(g_btnOk) || timedOut) {
        if (timedOut) Serial.println("(no selection made - starting automatically)");
        Serial.printf("CROP SELECTED: %s\n", CROPS[g_crop].name);
        Serial.println("--- ACQUIRING GPS FIX ---");
        g_acquireStartMs = millis();
        g_state = STATE_ACQUIRE;
      }
      // Amber while waiting for the farmer.
      digitalWrite(PIN_LED_YELLOW, (millis() / 400) % 2);
      break;
    }

    case STATE_ACQUIRE: {
      const uint32_t waited = millis() - g_acquireStartMs;
      // Wait for the satellite count too, so the first log row carries a
      // complete fix rather than a position with no quality attached.
      if (gps.location.isValid() && gps.satellites.isValid() && gps.satellites.value() > 0) {
        Serial.printf("GPS FIX ACQUIRED %.1f s after power-on: %.5f,%.5f (%d satellites)\n",
                      g_firstFixMs / 1000.0f, gps.location.lat(), gps.location.lng(),
                      (int)gps.satellites.value());
      } else if (waited > kFixTimeoutMs) {
        Serial.println("GPS FIX TIMEOUT - logging without position");
      } else {
        static uint32_t lastNote = 0;
        if (millis() - lastNote > 3000) {
          lastNote = millis();
          Serial.printf("  searching... %lus, %lu NMEA chars received\n",
                        (unsigned long)(waited / 1000), (unsigned long)gps.charsProcessed());
        }
        digitalWrite(PIN_LED_YELLOW, (millis() / 200) % 2);
        break;
      }

      // Rewind the simulated track to the farm gate so the first CSV row is the
      // point of dispatch, and let one NMEA cycle land before logging.
      simGpsResetJourney();
      for (uint32_t until = millis() + 1300; millis() < until;) {
        drainGps();
        delay(5);
      }

      Serial.println();
      Serial.printf("=== JOURNEY START : %s ===\n", CROPS[g_crop].name);
      g_journeyStartMs = millis();
      g_nextLogS = 0;
      sdLogStart();
      sendJourneyStart();
      sensorsResetPeak();
      g_state = STATE_LOGGING;
      break;
    }

    case STATE_LOGGING: {
      pollCustody();

      if (millis() - g_lastEvalMs >= 1000) {
        g_lastEvalMs = millis();
        evaluate(sensorsRead());
      }

      const uint32_t t = journeySeconds();
      if (t >= g_nextLogS) {
        const SensorReading r = sensorsRead();
        const int breaches =
            breachCount(cropEvaluate(CROPS[g_crop], r.tempC, r.humPct, r.shockG));
        g_stats.rows++;
        writeRow(r, breaches);
        sendHeartbeat(r, statusName(breaches));
        sensorsResetPeak();
        g_nextLogS = t + LOG_INTERVAL_JOURNEY_S;
      }

      if (pressed(g_btnOk) || t >= JOURNEY_TOTAL_S) {
        if (t >= JOURNEY_TOTAL_S) Serial.println("\n(mandi reached - closing the log)");
        printSummary();
        g_state = STATE_ENDED;
      }
      break;
    }

    case STATE_ENDED:
      // Log is closed; the farmer can reprint the summary at the mandi gate.
      if (pressed(g_btnOk)) printSummary();
      digitalWrite(PIN_LED_GREEN, (millis() / 800) % 2);
      break;
  }
}
