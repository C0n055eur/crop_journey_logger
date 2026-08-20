#include <Arduino.h>
#include <TinyGPSPlus.h>

TinyGPSPlus gps;
#define gpsSerial Serial2

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
    Serial.println("[GPS] Initialized");
    Serial.println("ESP32 started");
}

void loop() {
    Serial.println("Hello");
    delay(1000);
}

