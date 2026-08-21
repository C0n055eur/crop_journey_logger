#include "radio.h"
#include "protocol.h"

#ifndef SIM_BUILD
#define SIM_BUILD 1
#endif

// Prefix used on the simulated link so the bridge (and a human reading the
// serial log) can tell radio traffic apart from ordinary console output.
static const char *kSimPrefix = "LORA>";
static const size_t kSimPrefixLen = 5;

#if SIM_BUILD

void radioBegin() {
  // Careful not to print the prefix itself here: the bridge scans for it.
  Serial.println("RADIO: simulated LoRa over UART0, frames carry the radio tag");
}

void radioSend(const char *frame) {
  Serial.print(kSimPrefix);
  Serial.println(frame);
}

const char *radioReceive() {
  static char buf[CJL_FRAME_MAX];
  static size_t len = 0;

  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (len == 0) continue;
      buf[len] = '\0';
      len = 0;
      if (strncmp(buf, kSimPrefix, kSimPrefixLen) == 0) return buf + kSimPrefixLen;
      // Anything else on the console is not radio traffic; drop it.
      continue;
    }
    if (len < sizeof(buf) - 1) buf[len++] = c;
  }
  return nullptr;
}

#else  // ---------------------------------------------------------------- real

#include <LoRa.h>
#include <SPI.h>

// India ISM band. Overridable per environment in platformio.ini.
#ifndef LORA_FREQ
#define LORA_FREQ 865E6
#endif
#ifndef LORA_SS
#define LORA_SS 14
#endif
#ifndef LORA_RST
#define LORA_RST 17
#endif
#ifndef LORA_DIO0
#define LORA_DIO0 35
#endif

void radioBegin() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("RADIO: LoRa init FAILED");
    return;
  }
  // Long range over short payloads: the farm node may be 5-10 km away and we
  // only ever send ~100 bytes.
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();
  Serial.println("RADIO: LoRa ready");
}

void radioSend(const char *frame) {
  LoRa.beginPacket();
  LoRa.print(frame);
  LoRa.endPacket();
  LoRa.receive();
}

const char *radioReceive() {
  static char buf[CJL_FRAME_MAX];
  const int size = LoRa.parsePacket();
  if (size <= 0) return nullptr;

  size_t len = 0;
  while (LoRa.available() && len < sizeof(buf) - 1) buf[len++] = (char)LoRa.read();
  buf[len] = '\0';
  return buf;
}

#endif
