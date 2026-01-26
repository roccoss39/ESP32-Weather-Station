#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_SHT31.h"
#include "Adafruit_BME280.h"

// --- KONFIGURACJA SHT31 (z działającym pinem 33) ---
#define SHT_SDA 16
#define SHT_SCL 33
#define SHT_ADDR 0x44

// --- KONFIGURACJA BME280 (testowa para) ---
#define BME_SDA 26
#define BME_SCL 21
#define BME_ADDR 0x76  // Jeśli nie zadziała, spróbuj 0x77

Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_BME280 bme;

// Druga szyna I2C dla BME
TwoWire I2C_BME = TwoWire(1); 

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("\n\n=== TEST PORÓWNAWCZY SHT31 vs BME280 ===");

  // 1. Start SHT31 (Wire 0)
  Serial.print("Inicjalizacja SHT31 (Piny 16/33)... ");
  Wire.begin(SHT_SDA, SHT_SCL);
  if (sht31.begin(SHT_ADDR)) {
    Serial.println("✅ OK");
  } else {
    Serial.println("❌ BŁĄD");
  }

  // 2. Start BME280 (Wire 1)
  Serial.print("Inicjalizacja BME280 (Piny 26/21)... ");
  I2C_BME.begin(BME_SDA, BME_SCL);
  if (bme.begin(BME_ADDR, &I2C_BME)) {
    Serial.println("✅ OK");
  } else {
    Serial.println("❌ BŁĄD");
  }
  
  Serial.println("--------------------------------------------------");
}

void loop() {
  // Odczyty SHT31
  float t_sht = sht31.readTemperature();
  float h_sht = sht31.readHumidity();

  // Odczyty BME280
  float t_bme = bme.readTemperature();
  float h_bme = bme.readHumidity();
  float p_bme = bme.readPressure() / 100.0F;

  // Wyświetlanie w Monitorze
  Serial.print("[SHT31]  T: "); Serial.print(t_sht, 2); Serial.print(" C, H: "); Serial.print(h_sht, 1); Serial.println(" %");
  Serial.print("[BME280] T: "); Serial.print(t_bme, 2); Serial.print(" C, H: "); Serial.print(h_bme, 1); Serial.print(" %, P: "); Serial.print(p_bme, 1); Serial.println(" hPa");
  Serial.println("--------------------------------------------------");

  delay(2000);
}