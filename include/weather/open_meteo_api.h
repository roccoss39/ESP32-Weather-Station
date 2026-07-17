#ifndef OPEN_METEO_API_H
#define OPEN_METEO_API_H

#include <Arduino.h>

// Główna funkcja do pobierania danych z API Open-Meteo
void fetchOpenMeteoPressure();

// Gettery zapewniające bezpieczny dostęp do danych w zależności od trybu
const float* getOpenMeteoPressureHistory();
const float* getOpenMeteoPressureMslHistory();
const float* getOpenMeteoPressureSurfaceHistory();
bool isOpenMeteoDataValid();

// Dodana funkcja do przywracania danych wykresu z pamięci RTC (obsługuje obie serie)
void setOpenMeteoPressureHistory(const float* mslData, const float* surfData);

#endif // OPEN_METEO_API_H