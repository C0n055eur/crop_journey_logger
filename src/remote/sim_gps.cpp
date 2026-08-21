#include "sim_gps.h"
#include "../shared/journey.h"

#ifndef SIM_BUILD
#define SIM_BUILD 1
#endif

#if SIM_BUILD

#include <HardwareSerial.h>
#include <math.h>

namespace {

// Kolar farm -> Hoskote collection centre -> Yeshwanthpur APMC mandi.
// ~110 km, the shape of a real Karnataka vegetable run.
struct Waypoint {
  double lat;
  double lon;
};

const Waypoint kRoute[] = {
    {13.13600, 78.12900},  // farm gate, Kolar
    {13.15500, 77.96000},  // NH-75 towards Bengaluru
    {13.07070, 77.79800},  // collection centre, Hoskote
    {13.01000, 77.68000},  // KR Puram
    {13.02450, 77.55400},  // APMC mandi, Yeshwanthpur
};
const int kRouteLegs = (sizeof(kRoute) / sizeof(kRoute[0])) - 1;

// Cold-start delay before the receiver reports a fix. Well inside the "fix
// within 2 minutes of power-on" constraint, and long enough that acquisition is
// visible during a demo rather than instantaneous.
const uint32_t kTimeToFirstFixMs = 12000;

// Journey starts 06:00:00 UTC on 20 Aug 2026 - an early-morning mandi run.
const int kStartHour = 6, kStartMinute = 0, kStartSecond = 0;
const int kDay = 20, kMonth = 8, kYear = 26;

HardwareSerial gpsFeed(2);
volatile float g_timeScale = 1.0f;
volatile uint32_t g_journeyS = 0;
volatile bool g_resetRequested = false;

double legLengthDeg(int leg) {
  const double dLat = kRoute[leg + 1].lat - kRoute[leg].lat;
  const double dLon = (kRoute[leg + 1].lon - kRoute[leg].lon) * cos(kRoute[leg].lat * DEG_TO_RAD);
  return sqrt(dLat * dLat + dLon * dLon);
}

// Position at `t` seconds into the journey. Time is split between legs in
// proportion to their length, so the truck moves at a constant speed.
void positionAt(uint32_t t, double *lat, double *lon, double *speedKnots, double *courseDeg) {
  double total = 0;
  for (int i = 0; i < kRouteLegs; i++) total += legLengthDeg(i);

  const uint32_t clamped = t < JOURNEY_TOTAL_S ? t : JOURNEY_TOTAL_S;
  double travelled = total * (double)clamped / (double)JOURNEY_TOTAL_S;

  int leg = 0;
  while (leg < kRouteLegs - 1 && travelled > legLengthDeg(leg)) {
    travelled -= legLengthDeg(leg);
    leg++;
  }
  const double legLen = legLengthDeg(leg);
  const double frac = legLen > 0 ? travelled / legLen : 0.0;

  *lat = kRoute[leg].lat + (kRoute[leg + 1].lat - kRoute[leg].lat) * frac;
  *lon = kRoute[leg].lon + (kRoute[leg + 1].lon - kRoute[leg].lon) * frac;

  // 1 degree ~ 111.32 km; turn the whole-route pace into knots.
  const double km = total * 111.32;
  *speedKnots = (km / (JOURNEY_TOTAL_S / 3600.0)) / 1.852;

  const double dLat = kRoute[leg + 1].lat - kRoute[leg].lat;
  const double dLon = (kRoute[leg + 1].lon - kRoute[leg].lon) * cos(*lat * DEG_TO_RAD);
  double course = atan2(dLon, dLat) * RAD_TO_DEG;
  if (course < 0) course += 360.0;
  *courseDeg = course;
}

// ddmm.mmmm / dddmm.mmmm, the format a NEO-6M actually emits.
void formatDegMin(double value, int degWidth, char *out, size_t outLen, char *hemi, char positive,
                  char negative) {
  *hemi = value >= 0 ? positive : negative;
  value = fabs(value);
  const int deg = (int)value;
  const double minutes = (value - deg) * 60.0;
  snprintf(out, outLen, "%0*d%07.4f", degWidth, deg, minutes);
}

// NMEA checksum: XOR of everything between '$' and '*'.
void sendSentence(const char *body) {
  uint8_t sum = 0;
  for (const char *p = body; *p; p++) sum ^= (uint8_t)*p;
  gpsFeed.printf("$%s*%02X\r\n", body, sum);
}

void emit(uint32_t journeyS, bool haveFix) {
  const uint32_t tod = (uint32_t)kStartHour * 3600 + kStartMinute * 60 + kStartSecond + journeyS;
  const int hh = (tod / 3600) % 24, mm = (tod / 60) % 60, ss = tod % 60;

  char body[128];

  if (!haveFix) {
    // Searching: RMC void, GGA fix quality 0, no satellites.
    snprintf(body, sizeof(body), "GPRMC,%02d%02d%02d.00,V,,,,,,,%02d%02d%02d,,,N", hh, mm, ss, kDay,
             kMonth, kYear);
    sendSentence(body);
    snprintf(body, sizeof(body), "GPGGA,%02d%02d%02d.00,,,,,0,00,99.99,,,,,,", hh, mm, ss);
    sendSentence(body);
    return;
  }

  double lat, lon, speedKnots, course;
  positionAt(journeyS, &lat, &lon, &speedKnots, &course);

  char latStr[16], lonStr[16], ns, ew;
  formatDegMin(lat, 2, latStr, sizeof(latStr), &ns, 'N', 'S');
  formatDegMin(lon, 3, lonStr, sizeof(lonStr), &ew, 'E', 'W');

  snprintf(body, sizeof(body), "GPRMC,%02d%02d%02d.00,A,%s,%c,%s,%c,%05.1f,%05.1f,%02d%02d%02d,,,A",
           hh, mm, ss, latStr, ns, lonStr, ew, speedKnots, course, kDay, kMonth, kYear);
  sendSentence(body);

  snprintf(body, sizeof(body), "GPGGA,%02d%02d%02d.00,%s,%c,%s,%c,1,09,0.9,%.1f,M,-16.0,M,,", hh,
           mm, ss, latStr, ns, lonStr, ew, 850.0 - journeyS * 0.01);
  sendSentence(body);
}

void feedTask(void *) {
  const uint32_t startMs = millis();
  float journeyS = 0;
  TickType_t last = xTaskGetTickCount();

  for (;;) {
    if (g_resetRequested) {
      g_resetRequested = false;
      journeyS = 0;
    }
    const bool haveFix = (millis() - startMs) >= kTimeToFirstFixMs;
    if (haveFix) journeyS += g_timeScale;  // one sentence pair per real second
    g_journeyS = (uint32_t)journeyS;

    emit((uint32_t)journeyS, haveFix);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
  }
}

}  // namespace

void simGpsBegin(int txPin, uint32_t baud) {
  gpsFeed.begin(baud, SERIAL_8N1, /*rx=*/-1, txPin);
  xTaskCreatePinnedToCore(feedTask, "sim_gps", 4096, nullptr, 1, nullptr, APP_CPU_NUM);
}

void simGpsSetTimeScale(float scale) { g_timeScale = scale; }

void simGpsResetJourney() { g_resetRequested = true; }

uint32_t simGpsJourneySeconds() { return g_journeyS; }

uint32_t simGpsTimeToFirstFixMs() { return kTimeToFirstFixMs; }

#else  // real hardware: the NEO-6M does all of this itself.

void simGpsBegin(int, uint32_t) {}
void simGpsSetTimeScale(float) {}
void simGpsResetJourney() {}
uint32_t simGpsJourneySeconds() { return 0; }
uint32_t simGpsTimeToFirstFixMs() { return 0; }

#endif
