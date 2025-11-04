#ifndef FORECAST_DATA_H
#define FORECAST_DATA_H

#include <Arduino.h>

// --- STRUKTURA POJEDYNCZEJ PROGNOZY ---
struct ForecastItem {
  String time = "";           // "15:00"
  float temperature = 0;      // temperatura
  float windSpeed = 0;        // prędkość wiatru w m/s
  String icon = "";           // kod ikony API
  String description = "";    // opis (dla debug)
  int precipitationChance = 0; // Prawdopodobieństwo opadów (0-100%)
};

// --- STRUKTURA PROGNOZY 3H ---
struct ForecastData {
  ForecastItem items[5];      // 5 prognoz co 3h
  int count = 0;              // ile prognoz jest dostępnych
  bool isValid = false;
  unsigned long lastUpdate = 0;
};

// Globalna zmienna prognozy
extern ForecastData forecast;
extern unsigned long lastForecastUpdate;

#endif