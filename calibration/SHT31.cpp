#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// --- KONFIGURACJA PINÓW I2C (DOMYŚLNE DLA ESP32) ---
#define I2C_SDA 21
#define I2C_SCL 22

// Adres I2C czujnika SHT31 (zazwyczaj 0x44, czasem 0x45 jeśli pin ADDR jest podłączony do VCC)
#define SHT31_ADDR 0x44

Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); // Czekaj na konsolę

  Serial.println("\n=== TEST CZUJNIKA SHT31 ===");
  Serial.println("Inicjalizacja I2C...");

  // Inicjalizacja Wire na konkretnych pinach
  Wire.begin(I2C_SDA, I2C_SCL);

  // Próba połączenia z czujnikiem
  if (!sht31.begin(SHT31_ADDR)) {
    Serial.println("❌ BLAD: Nie znaleziono SHT31!");
    Serial.println("Sprawdz podlaczenie:");
    Serial.printf("SDA -> GPIO %d\n", I2C_SDA);
    Serial.printf("SCL -> GPIO %d\n", I2C_SCL);
    Serial.println("VCC -> 3.3V");
    Serial.println("GND -> GND");
    while (1) delay(1000); // Zatrzymaj program
  }

  Serial.println("✅ SHT31 znaleziony i gotowy!");
  Serial.print("Status grzalki: ");
  Serial.println(sht31.isHeaterEnabled() ? "WLACZONA" : "WYLACZONA");
  Serial.println("-----------------------------");
}

void loop() {
  // Pobieranie danych
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  // Sprawdzenie błędów odczytu (NaN = Not a Number)
  if (!isnan(t) && !isnan(h)) {
    Serial.print("Temp: "); 
    Serial.print(t); 
    Serial.print(" °C\t");
    
    Serial.print("Wilg: "); 
    Serial.print(h); 
    Serial.println(" %");
  } else { 
    Serial.println("⚠️ Blad odczytu danych!"); 
  }

  delay(2000); // Odczyt co 2 sekundy
}