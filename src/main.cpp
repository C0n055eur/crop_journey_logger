#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Define DHT parameters
#define DHTPIN 4          // GPIO connected to DHT22
#define DHTTYPE DHT22     // DHT type (DHT11 or DHT22)
DHT dht(DHTPIN, DHTTYPE);

// Create GPS and Serial objects
TinyGPSPlus gps;
HardwareSerial gpsSerial(1); // Use UART1 for GPS

// Create MPU6050 object
Adafruit_MPU6050 mpu;

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("GPS Neo-6M + Temperature Sensor + MPU6050");

  // Initialize GPS UART
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17); // RX = GPIO16, TX = GPIO17

  // Initialize DHT sensor
  dht.begin();

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip!");
    while (1);
  }
  Serial.println("MPU6050 connected successfully!");

  // Configure MPU6050
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}


void displayData() {
  // GPS Data
  if (true) {
    Serial.print("Latitude: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(", Longitude: ");
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println("No GPS");
  }
  
  // Temperature Data
  float temperature = dht.readTemperature();
  if (isnan(temperature)) {
    Serial.println("Error reading temperature!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
  }
  
  // MPU6050 Data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  // Print Accelerometer data
  Serial.print("Accel X: ");
  Serial.print(a.acceleration.x);
  Serial.print(" m/s^2, Y: ");
  Serial.print(a.acceleration.y);
  Serial.print(" m/s^2, Z: ");
  Serial.print(a.acceleration.z);
  Serial.println(" m/s^2");
  
  // Print Gyroscope data
  Serial.print("Gyro X: ");
  Serial.print(g.gyro.x);
  Serial.print(" rad/s, Y: ");
  Serial.print(g.gyro.y);
  Serial.print(" rad/s, Z: ");
  Serial.print(g.gyro.z);
  Serial.println(" rad/s");
  
  // Optional: Add GPS timestamp
  if (gps.time.isUpdated()) {
    Serial.print("Time: ");
    Serial.print(gps.time.hour());
    Serial.print(":");
    Serial.print(gps.time.minute());
    Serial.print(":");
    Serial.println(gps.time.second());
  }

  Serial.println("---------------------");
}

void loop() {
  // Read GPS data
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gps.encode(c); // Process the character
  }

  // Display data
  displayData();
  delay(2000); // Delay for 2 seconds
}