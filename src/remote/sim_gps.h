#pragma once
#include <Arduino.h>

// Simulated NEO-6M.
//
// Wokwi has no GPS part, so in SIM builds we generate real NMEA sentences and
// push them out of UART2 on `txPin`. In diagram.json that pin is wired straight
// back to the GPS RX pin (GPIO16), so the bytes travel over a genuine UART and
// TinyGPS++ parses them exactly as it would parse a real NEO-6M. Nothing above
// the UART knows the difference.
//
// On real hardware this whole unit compiles away; GPIO17 becomes LoRa RST and
// the NEO-6M's TX drives GPIO16 instead.

// Starts the generator task. Call once from setup().
void simGpsBegin(int txPin, uint32_t baud = 9600);

// Journey time compression, matching TIME_SCALE in shared/journey.h.
void simGpsSetTimeScale(float scale);

// Put the truck back at the farm gate at 06:00. Called when the journey
// actually starts, so the NMEA clock and the journey clock agree.
void simGpsResetJourney();

// Seconds elapsed along the simulated route (journey time, not wall clock).
uint32_t simGpsJourneySeconds();

// Milliseconds from power-on to the first valid fix, for the time-to-fix check.
uint32_t simGpsTimeToFirstFixMs();
