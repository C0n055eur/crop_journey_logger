#pragma once
#include <Arduino.h>

// The only part of the system that knows whether the link is a real SX1276 or
// the simulator.
//
// On hardware (SIM_BUILD=0) these call sandeepmistry/LoRa and packets go out
// over 865 MHz. In Wokwi, which has no LoRa part, the identical frame is
// written to UART0 prefixed with "LORA>" and read back the same way, so
// tools/lora_bridge.py can carry frames from the truck simulation to the farm
// simulation. Everything above this header is unchanged between the two.

void radioBegin();

// Transmit one frame. `frame` must be a NUL-terminated CJL frame.
void radioSend(const char *frame);

// Non-blocking receive. Returns a pointer to a NUL-terminated frame, or
// nullptr if nothing has arrived. The buffer is reused on the next call.
const char *radioReceive();
