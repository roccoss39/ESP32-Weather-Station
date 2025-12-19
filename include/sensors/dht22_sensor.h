#ifndef DHT22_SENSOR_H
#define DHT22_SENSOR_H

#include <Arduino.h>

// === KONFIGURACJA DHT22 ===
#include "config/hardware_config.h"  // DHT22_PIN moved to hardware_config.h
#define DHT22_READ_INTERVAL 2000  // Odczyt co 2 sekundy

// === STRUKTURA DANYCH DHT22 ===
struct DHT22Data {
  float temperature = -999.0;  // Temperatura w °C
  float humidity = -999.0;     // Wilgotność w %
  bool isValid = false;        // Czy dane są prawidłowe
  unsigned long lastUpdate = 0; // Czas ostatniego odczytu
  String status = "Inicjalizacja"; // Status czujnika
};

// === KLASA ZARZĄDZANIA DHT22 ===
class DHT22Sensor {
private:
  DHT22Data currentData;
  unsigned long lastReadTime = 0;
  
public:
  // Inicjalizacja czujnika
  void init();
  
  // Odczyt danych z czujnika
  void readSensor();
  
  // Pobierz aktualne dane
  DHT22Data getCurrentData();
  
  // Sprawdź czy dane są świeże (< 10s)
  bool isDataFresh();
  
  // Aktualizuj status czujnika
  void updateStatus();
};

// === FUNKCJE GLOBALNE ===
extern DHT22Sensor dht22;

void initDHT22();
void updateDHT22();
DHT22Data getDHT22Data();

#endif