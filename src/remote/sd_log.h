#pragma once
#include <Arduino.h>

// The journey log itself: one plain CSV file a judge, a trader or an insurer
// can open in Excel without any tooling.

#define JOURNEY_CSV_PATH "/JOURNEY.CSV"

bool sdLogBegin(uint8_t csPin);
bool sdLogAvailable();

// Truncates any previous run and writes the header row.
bool sdLogStart();

// Appends one row (no trailing newline needed).
void sdLogRow(const char *row);

// Wokwi gives no way to pull a file off the simulated card, so the whole log is
// echoed to the console on demand. This is also what the CI scenario asserts on.
void sdLogDumpToSerial();
