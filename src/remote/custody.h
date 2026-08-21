#pragma once
#include <Arduino.h>

// RFID chain of custody.
//
// Each waypoint - farm gate, collection centre, mandi - has its own tag. The
// first distinct tag seen takes the FARM role, the second COLLECTION, the third
// MANDI, and any further tag is recorded as an extra handover. That ordering is
// what makes the demo work with whichever preset cards Wokwi offers, and on real
// hardware it means tags can be handed out without pre-programming the device.

bool custodyBegin(uint8_t ssPin, uint8_t rstPin);

// Poll for a scan. Returns the role index (0..) of a newly seen tag, or -1.
// `uidOut` receives the hex UID.
int custodyPoll(char *uidOut, size_t uidLen);

// Role name of the party currently holding the consignment, "" before the
// first scan.
const char *custodyCurrent();

int custodyCount();
const char *custodyRoleName(int index);
uint32_t custodyStampS(int index);
void custodyRecordStamp(int index, uint32_t journeyS);
