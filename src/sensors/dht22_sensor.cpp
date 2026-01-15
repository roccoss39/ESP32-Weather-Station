#include "sensors/dht22_sensor.h"
#include <DHT.h>
#include "config/hardware_config.h" // Tutaj są zdefiniowane DHT_PIN i DHT_TYPE

// === INSTANCJA GLOBALNA ===
DHT22Sensor dht22;

// === INSTANCJA DHT BIBLIOTEKI ===
// POPRAWKA: Używamy DHT_PIN i DHT_TYPE z pliku hardware_config.h
DHT dhtSensor(DHT_PIN, DHT_TYPE);

// === IMPLEMENTACJA KLASY DHT22Sensor ===

void DHT22Sensor::init() {
  Serial.println("🌡️ Inicjalizacja czujnika DHT22...");
  
  // Inicjalizuj bibliotekę DHT
  dhtSensor.begin();
  
  currentData.temperature = -999.0;
  currentData.humidity = -999.0;
  currentData.isValid = false;
  currentData.status = "Inicjalizacja";
  currentData.lastUpdate = 0;
  
  // POPRAWKA: Używamy DHT_PIN zamiast starego DHT22_PIN
  Serial.printf("🌡️ DHT22 skonfigurowany na pinie %d\n", DHT_PIN);
  
  // Pierwsze czytanie po 2 sekundach
  lastReadTime = millis();
}

void DHT22Sensor::readSensor() {
  // Sprawdź czy minął odpowiedni czas
  if (millis() - lastReadTime < DHT22_READ_INTERVAL) {
    return; // Za wcześnie na kolejny odczyt
  }
  
  lastReadTime = millis();
  
  // Serial.println("🌡️ Odczytywanie danych z DHT22..."); // Można odkomentować do debugowania
  
  // PRAWDZIWY ODCZYT Z DHT22
  float temp = dhtSensor.readTemperature();
  float hum = dhtSensor.readHumidity();
  
  // Sprawdź poprawność danych (NaN oznacza błąd odczytu)
  if (!isnan(temp) && !isnan(hum) && 
      temp >= -40.0 && temp <= 80.0 && 
      hum >= 0.0 && hum <= 100.0) {
    
    currentData.temperature = temp;
    currentData.humidity = hum;
    currentData.isValid = true;
    currentData.status = "OK";
    currentData.lastUpdate = millis();
    
    // Serial.printf("🌡️ DHT22 prawdziwy odczyt: %.1f°C, %.1f%%\n", temp, hum);
    
  } else {
    // Błędne dane lub błąd komunikacji
    currentData.isValid = false;
    if (isnan(temp) || isnan(hum)) {
      currentData.status = "Blad komunikacji";
      Serial.println("❌ DHT22: Błąd komunikacji z czujnikiem (sprawdź połączenia)");
    } else {
      currentData.status = "Dane poza zakresem";
      Serial.printf("❌ DHT22: Dane poza zakresem - temp:%.1f°C, hum:%.1f%%\n", temp, hum);
    }
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