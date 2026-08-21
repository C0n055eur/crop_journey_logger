// Farm-to-Mandi Journey Logger - base (farm) node.
//
// Sits at the farm with an OLED and a LoRa receiver. It never decides anything:
// it renders what the truck reports. Alerts arrive the moment a threshold is
// crossed; the end-of-journey summary arrives when the farmer presses OK in the
// truck.

#include <Arduino.h>

#include "model.h"
#include "oled_ui.h"
#include "../shared/journey.h"
#include "../shared/protocol.h"
#include "../shared/radio.h"

#define PIN_BTN_PAGE 32
#define PIN_LED_ALERT 25

namespace {

BaseModel g_model;
Screen g_screen = SCREEN_LINK;
uint32_t g_lastDrawMs = 0;
uint32_t g_alertFlashUntilMs = 0;
bool g_dirty = true;

bool g_btnLast = true;
uint32_t g_btnChangedMs = 0;

bool pagePressed() {
  const bool level = digitalRead(PIN_BTN_PAGE);
  const uint32_t now = millis();
  if (level != g_btnLast && now - g_btnChangedMs > 40) {
    g_btnChangedMs = now;
    g_btnLast = level;
    return level == LOW;
  }
  return false;
}

void pushAlert(const CjlFrame &f) {
  // Newest first; the oldest falls off the end.
  for (int i = BASE_ALERT_HISTORY - 1; i > 0; i--) g_model.alerts[i] = g_model.alerts[i - 1];

  BaseAlert &a = g_model.alerts[0];
  strncpy(a.metric, f.str(5), sizeof(a.metric) - 1);
  a.metric[sizeof(a.metric) - 1] = '\0';
  a.value = f.num(6);
  a.limit = f.num(7);
  a.lat = f.num(8);
  a.lon = f.num(9);
  a.journeyS = f.integer(10);

  if (g_model.alertCount < BASE_ALERT_HISTORY * 1000) g_model.alertCount++;

  Serial.printf("ALERT #%d  %s %.2f (limit %.2f) at %.5f,%.5f t+%lus\n", g_model.alertCount,
                a.metric, a.value, a.limit, a.lat, a.lon, (unsigned long)a.journeyS);

  // An alert is the one thing that should interrupt whatever the farmer is
  // looking at.
  g_screen = SCREEN_ALERT;
  g_alertFlashUntilMs = millis() + 4000;
}

void handleFrame(const CjlFrame &f) {
  g_model.packets++;
  g_model.lastRxMs = millis();

  switch (f.type()) {
    case CJL_JOURNEY_START:
      g_model = BaseModel();  // a new consignment; drop the last one's history
      g_model.packets = 1;
      g_model.lastRxMs = millis();
      g_model.journeyActive = true;
      strncpy(g_model.crop, f.str(4), sizeof(g_model.crop) - 1);
      g_model.tempLimit = f.num(5);
      g_model.humMin = f.num(6);
      g_model.humMax = f.num(7);
      g_model.shockLimit = f.num(8);
      Serial.printf("JOURNEY START: %s (max %.1fC, %.0f-%.0f%%, %.1fg)\n", g_model.crop,
                    g_model.tempLimit, g_model.humMin, g_model.humMax, g_model.shockLimit);
      g_screen = SCREEN_LINK;
      break;

    case CJL_HEARTBEAT:
      g_model.haveLive = true;
      strncpy(g_model.crop, f.str(4), sizeof(g_model.crop) - 1);
      g_model.temp = f.num(5);
      g_model.hum = f.num(6);
      g_model.shock = f.num(7);
      strncpy(g_model.status, f.str(8), sizeof(g_model.status) - 1);
      g_model.status[sizeof(g_model.status) - 1] = '\0';
      g_model.lat = f.num(9);
      g_model.lon = f.num(10);
      g_model.journeyS = f.integer(11);
      break;

    case CJL_ALERT:
      pushAlert(f);
      break;

    case CJL_CUSTODY: {
      if (g_model.custodyCount < BASE_CUSTODY_MAX) {
        BaseCustody &c = g_model.custody[g_model.custodyCount++];
        strncpy(c.role, f.str(4), sizeof(c.role) - 1);
        c.role[sizeof(c.role) - 1] = '\0';
        c.journeyS = f.integer(8);
        Serial.printf("CUSTODY: %s at t+%lus (uid %s)\n", c.role, (unsigned long)c.journeyS,
                      f.str(5));
      }
      g_screen = SCREEN_CUSTODY;
      break;
    }

    case CJL_SUMMARY:
      g_model.haveSummary = true;
      g_model.journeyActive = false;
      strncpy(g_model.crop, f.str(4), sizeof(g_model.crop) - 1);
      g_model.sTempMax = f.num(5);
      g_model.sTempMin = f.num(6);
      g_model.sHumMin = f.num(7);
      g_model.sHumMax = f.num(8);
      g_model.sShockMax = f.num(9);
      g_model.sExcursions = (uint16_t)f.integer(10);
      g_model.sDurationS = f.integer(11);
      strncpy(g_model.sGrade, f.str(12), sizeof(g_model.sGrade) - 1);
      g_model.sGrade[sizeof(g_model.sGrade) - 1] = '\0';
      Serial.printf(
          "JOURNEY SUMMARY: %s max %.1fC, %.0f-%.0f%%, %.2fg, %u excursions, %luh%02lum, "
          "verdict %s\n",
          g_model.crop, g_model.sTempMax, g_model.sHumMin, g_model.sHumMax, g_model.sShockMax,
          g_model.sExcursions, (unsigned long)(g_model.sDurationS / 3600),
          (unsigned long)((g_model.sDurationS % 3600) / 60), g_model.sGrade);
      g_screen = SCREEN_SUMMARY;
      break;

    default:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== Farm-to-Mandi Journey Logger : BASE NODE ===");

  pinMode(PIN_BTN_PAGE, INPUT_PULLUP);
  pinMode(PIN_LED_ALERT, OUTPUT);

  if (!oledBegin()) {
    Serial.println("OLED: SSD1306 not found at 0x3C");
  } else {
    oledSplash();
  }

  radioBegin();
  Serial.println("BASE: listening. [PAGE] cycles LINK / ALERT / SUMMARY / CUSTODY");
}

void loop() {
  const char *line = radioReceive();
  if (line) {
    CjlFrame f;
    if (cjlParse(line, f)) {
      handleFrame(f);
      g_dirty = true;
    }
  }

  if (pagePressed()) {
    g_screen = (Screen)((g_screen + 1) % SCREEN_COUNT);
    g_dirty = true;
  }

  const bool flashing = millis() < g_alertFlashUntilMs;
  digitalWrite(PIN_LED_ALERT, flashing ? ((millis() / 150) % 2) : LOW);

  // Redraw on change, plus a slow tick so the "last rx" age keeps counting.
  // Blindly repainting 1 KB of framebuffer several times a second buys nothing
  // and costs the whole I2C bus.
  if (g_dirty || millis() - g_lastDrawMs >= 1000) {
    g_dirty = false;
    g_lastDrawMs = millis();
    oledDraw(g_screen, g_model);
  }
}
