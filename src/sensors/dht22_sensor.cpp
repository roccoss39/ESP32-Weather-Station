#include "sensors/dht22_sensor.h"

// === INSTANCJA GLOBALNA ===
DHT22Sensor dht22;

// === IMPLEMENTACJA KLASY DHT22Sensor ===

void DHT22Sensor::init() {
  Serial.println("🌡️ Inicjalizacja czujnika DHT22...");
  
  pinMode(DHT22_PIN, INPUT_PULLUP);
  
  currentData.temperature = -999.0;
  currentData.humidity = -999.0;
  currentData.isValid = false;
  currentData.status = "Inicjalizacja";
  currentData.lastUpdate = 0;
  
  Serial.printf("🌡️ DHT22 skonfigurowany na pinie %d\n", DHT22_PIN);
  
  // Pierwsze czytanie po 2 sekundach
  lastReadTime = millis();
}

void DHT22Sensor::readSensor() {
  // Sprawdź czy minął odpowiedni czas
  if (millis() - lastReadTime < DHT22_READ_INTERVAL) {
    return; // Za wcześnie na kolejny odczyt
  }
  
  lastReadTime = millis();
  
  Serial.println("🌡️ Odczytywanie danych z DHT22...");
  
  // TODO: Implementacja odczytu DHT22
  // Na razie symulowane dane dla testów
  
  // Symulowane dane (losowe w realistycznych zakresach)
  float simTemp = 20.0 + (random(-50, 150) / 10.0);  // 15-35°C
  float simHum = 45.0 + (random(-200, 300) / 10.0);  // 25-75%
  
  // Sprawdź poprawność danych
  if (simTemp >= -40.0 && simTemp <= 80.0 && 
      simHum >= 0.0 && simHum <= 100.0) {
    
    currentData.temperature = simTemp;
    currentData.humidity = simHum;
    currentData.isValid = true;
    currentData.status = "OK";
    currentData.lastUpdate = millis();
    
    Serial.printf("🌡️ DHT22 odczyt: %.1f°C, %.1f%%\n", simTemp, simHum);
    
  } else {
    // Błędne dane
    currentData.isValid = false;
    currentData.status = "Blad odczytu";
    Serial.println("❌ DHT22: Błędne dane z czujnika");
  }
}

DHT22Data DHT22Sensor::getCurrentData() {
  return currentData;
}

bool DHT22Sensor::isDataFresh() {
  return (millis() - currentData.lastUpdate) < 10000; // 10 sekund
}

void DHT22Sensor::updateStatus() {
  if (!currentData.isValid) {
    currentData.status = "Blad";
  } else if (!isDataFresh()) {
    currentData.status = "Stare dane";
  } else {
    currentData.status = "OK";
  }
}

// === FUNKCJE GLOBALNE ===

void initDHT22() {
  dht22.init();
}

void updateDHT22() {
  dht22.readSensor();
  dht22.updateStatus();
}

DHT22Data getDHT22Data() {
  return dht22.getCurrentData();
}