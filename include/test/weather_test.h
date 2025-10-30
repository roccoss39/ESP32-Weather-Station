#ifndef WEATHER_TEST_H
#define WEATHER_TEST_H

// --- FUNKCJE TESTOWE ---
void initWeatherTest();
void runWeatherTest();
void resetWeatherTest();
bool validateWeatherData();

// --- KONFIGURACJA TESTÓW ---
extern const unsigned long TEST_INTERVAL; // Interwał zmiany testów (ms)

#endif