#include "sensors/sht31_sensor.h"
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include "config/hardware_config.h"

// Definicja zmiennych globalnych
float localTemperature = 0.0;
float localHumidity = 0.0;

Adafruit_SHT31 sht = Adafruit_SHT31();

unsigned long lastSHTUpdate = 0;
const unsigned long SHT_UPDATE_INTERVAL = 1000; // Odświeżanie co 1 sek

void initSHT31() {
    Serial.println("--- INIT SHT31 ---");
    
    // Inicjalizacja I2C na zdefiniowanych pinach
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!sht.begin(0x44)) {   // 0x44 to domyślny adres
        Serial.println("❌ Błąd: Nie wykryto SHT31!");
    } else {
        Serial.println("✅ SHT31 zainicjalizowany poprawnie.");
    }
}

void updateSHT31() {
    if (millis() - lastSHTUpdate >= SHT_UPDATE_INTERVAL) {
        lastSHTUpdate = millis();

        float t = sht.readTemperature();
        float h = sht.readHumidity();

        if (!isnan(t)) {
            localTemperature = t;
        } else {
            Serial.println("⚠️ Błąd odczytu Temp SHT31");
        }

        if (!isnan(h)) {
            localHumidity = h;
        } else {
            Serial.println("⚠️ Błąd odczytu Wilg SHT31");
        }
        
        // Debug (opcjonalnie)
        // Serial.printf("SHT31 -> Temp: %.2f *C, Hum: %.2f %%\n", localTemperature, localHumidity);
    }
}