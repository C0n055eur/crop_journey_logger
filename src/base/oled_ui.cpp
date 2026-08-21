#include "oled_ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

namespace {

const int kWidth = 128;
const int kHeight = 64;
Adafruit_SSD1306 display(kWidth, kHeight, &Wire, -1);

void header(const char *title, const BaseModel &m) {
  display.fillRect(0, 0, kWidth, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 2);
  display.print(title);
  display.setCursor(kWidth - 40, 2);
  display.print(m.crop);
  display.setTextColor(SSD1306_WHITE);
}

void hhmm(uint32_t s, char *out, size_t len) {
  snprintf(out, len, "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
}

void drawLink(const BaseModel &m) {
  header("LINK", m);
  display.setCursor(0, 15);

  const uint32_t ageS = m.packets ? (millis() - m.lastRxMs) / 1000 : 0;
  display.printf("packets  %lu\n", (unsigned long)m.packets);
  if (!m.packets) {
    display.println("waiting for truck...");
    return;
  }
  display.printf("last rx  %lus ago\n", (unsigned long)ageS);

  if (m.haveLive) {
    char t[12];
    hhmm(m.journeyS, t, sizeof(t));
    display.printf("t+%-6s %s\n", t, m.status);
    display.printf("%.1fC %.0f%% %.2fg\n", m.temp, m.hum, m.shock);
    display.printf("%.4f,%.4f", m.lat, m.lon);
  } else if (m.journeyActive) {
    display.println("journey started");
    display.printf("limits %.0fC %.1fg", m.tempLimit, m.shockLimit);
  }
}

void drawAlert(const BaseModel &m) {
  header("ALERT", m);
  if (m.alertCount == 0) {
    display.setCursor(0, 26);
    display.println(" no alerts - clean run");
    return;
  }

  const BaseAlert &a = m.alerts[0];
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.printf("%s\n", a.metric);
  display.printf("%.1f\n", a.value);
  display.setTextSize(1);

  char t[12];
  hhmm(a.journeyS, t, sizeof(t));
  display.setCursor(0, 47);
  display.printf("limit %.1f  at t+%s\n", a.limit, t);
  display.printf("%.4f,%.4f  (%d)", a.lat, a.lon, m.alertCount);
}

void drawSummary(const BaseModel &m) {
  header("SUMMARY", m);
  display.setCursor(0, 15);
  if (!m.haveSummary) {
    display.println("journey still running");
    display.println();
    display.println("summary arrives when");
    display.println("the farmer presses OK");
    return;
  }
  char t[12];
  hhmm(m.sDurationS, t, sizeof(t));
  display.printf("max temp  %.1f C\n", m.sTempMax);
  display.printf("humidity  %.0f-%.0f%%\n", m.sHumMin, m.sHumMax);
  display.printf("max shock %.2f g\n", m.sShockMax);
  display.printf("excursion %u\n", m.sExcursions);
  display.printf("duration  %s\n", t);
  display.drawRect(0, 54, kWidth, 10, SSD1306_WHITE);
  display.setCursor(3, 56);
  display.printf("VERDICT %s", m.sGrade);
}

void drawCustody(const BaseModel &m) {
  header("CUSTODY", m);
  display.setCursor(0, 15);
  if (m.custodyCount == 0) {
    display.println("no waypoints scanned");
    return;
  }
  for (int i = 0; i < m.custodyCount && i < 5; i++) {
    char t[12];
    hhmm(m.custody[i].journeyS, t, sizeof(t));
    display.printf("%-11s %s\n", m.custody[i].role, t);
  }
}

}  // namespace

bool oledBegin() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) return false;
  // A full 128x64 frame is 1 KB over I2C; at the 100 kHz default that is 80 ms
  // of bus time per redraw.
  Wire.setClock(400000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  return true;
}

void oledSplash() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(6, 12);
  display.println("FARM BASE NODE");
  display.setCursor(6, 26);
  display.println("Journey Logger");
  display.setCursor(6, 44);
  display.println("listening...");
  display.display();
}

void oledDraw(Screen screen, const BaseModel &m) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  switch (screen) {
    case SCREEN_LINK: drawLink(m); break;
    case SCREEN_ALERT: drawAlert(m); break;
    case SCREEN_SUMMARY: drawSummary(m); break;
    case SCREEN_CUSTODY: drawCustody(m); break;
    default: break;
  }
  display.display();
}
