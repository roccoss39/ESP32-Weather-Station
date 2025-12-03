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

// --- STRUKTURA POJEDYNCZEGO DNIA (5-DNIOWA PROGNOZA) ---
struct DailyForecast {
  String dayName = "";        // "Pon", "Wto", "Sro", etc.
  float tempMin = 0;          // minimalna temperatura dnia
  float tempMax = 0;          // maksymalna temperatura dnia
  float windMin = 0;          // minimalny wiatr dnia (km/h)
  float windMax = 0;          // maksymalny wiatr dnia (km/h)
  String icon = "";           // najczęstsza ikona dnia
  String description = "";    // główny opis pogody
  int precipitationChance = 0; // średnie prawdopodobieństwo opadów
};

// --- STRUKTURA PROGNOZY 5-DNIOWEJ ---
struct WeeklyForecastData {
  DailyForecast days[5];      // 5 dni prognozy
  int count = 0;              // ile dni jest dostępnych
  bool isValid = false;
  unsigned long lastUpdate = 0;
};

// Globalne zmienne prognozy
extern ForecastData forecast;
extern WeeklyForecastData weeklyForecast;
extern unsigned long lastForecastUpdate;
extern unsigned long lastWeeklyUpdate;

#endif