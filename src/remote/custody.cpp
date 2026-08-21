#include "custody.h"
#include "../shared/journey.h"

#include <MFRC522.h>
#include <SPI.h>

namespace {

const int kMaxWaypoints = 6;

MFRC522 *g_reader = nullptr;

struct Waypoint {
  char uid[24];
  char role[16];
  uint32_t journeyS;
};

Waypoint g_seen[kMaxWaypoints];
int g_count = 0;

void uidToHex(const MFRC522::Uid &uid, char *out, size_t outLen) {
  size_t pos = 0;
  for (uint8_t i = 0; i < uid.size && pos + 3 < outLen; i++) {
    pos += snprintf(out + pos, outLen - pos, i ? ":%02X" : "%02X", uid.uidByte[i]);
  }
  out[pos] = '\0';
}

int findUid(const char *uid) {
  for (int i = 0; i < g_count; i++) {
    if (strcmp(g_seen[i].uid, uid) == 0) return i;
  }
  return -1;
}

}  // namespace

bool custodyBegin(uint8_t ssPin, uint8_t rstPin) {
  SPI.begin();
  static MFRC522 reader(ssPin, rstPin);
  g_reader = &reader;
  g_reader->PCD_Init();
  delay(20);
  const byte version = g_reader->PCD_ReadRegister(MFRC522::VersionReg);
  const bool ok = version != 0x00 && version != 0xFF;
  Serial.printf("RFID: MFRC522 %s (version 0x%02X)\n", ok ? "ready" : "not detected", version);
  return ok;
}

int custodyPoll(char *uidOut, size_t uidLen) {
  if (!g_reader) return -1;
  if (!g_reader->PICC_IsNewCardPresent()) return -1;
  if (!g_reader->PICC_ReadCardSerial()) return -1;

  char uid[24];
  uidToHex(g_reader->uid, uid, sizeof(uid));
  g_reader->PICC_HaltA();

  if (findUid(uid) >= 0) return -1;  // same custodian, already recorded
  if (g_count >= kMaxWaypoints) return -1;

  const int index = g_count++;
  strncpy(g_seen[index].uid, uid, sizeof(g_seen[index].uid) - 1);
  g_seen[index].uid[sizeof(g_seen[index].uid) - 1] = '\0';
  if (index < CUSTODY_ROLE_COUNT) {
    strncpy(g_seen[index].role, CUSTODY_ROLES[index], sizeof(g_seen[index].role) - 1);
  } else {
    snprintf(g_seen[index].role, sizeof(g_seen[index].role), "HANDOVER%d", index - 2);
  }
  g_seen[index].role[sizeof(g_seen[index].role) - 1] = '\0';
  g_seen[index].journeyS = 0;

  if (uidOut) {
    strncpy(uidOut, uid, uidLen - 1);
    uidOut[uidLen - 1] = '\0';
  }
  return index;
}

const char *custodyCurrent() { return g_count > 0 ? g_seen[g_count - 1].role : ""; }

int custodyCount() { return g_count; }

const char *custodyRoleName(int index) {
  return (index >= 0 && index < g_count) ? g_seen[index].role : "";
}

uint32_t custodyStampS(int index) {
  return (index >= 0 && index < g_count) ? g_seen[index].journeyS : 0;
}

void custodyRecordStamp(int index, uint32_t journeyS) {
  if (index >= 0 && index < g_count) g_seen[index].journeyS = journeyS;
}
