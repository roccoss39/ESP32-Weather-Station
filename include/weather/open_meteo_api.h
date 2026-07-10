#ifndef OPEN_METEO_API_H
#define OPEN_METEO_API_H

#include <Arduino.h>

// Główna funkcja do pobierania danych z API Open-Meteo
void fetchOpenMeteoPressure();

// Gettery zapewniające bezpieczny dostęp (tylko do odczytu) do danych
const float* getOpenMeteoPressureHistory();
bool isOpenMeteoDataValid();

// Dodana funkcja do przywracania danych wykresu z pamięci RTC
void setOpenMeteoPressureHistory(const float* data);

#endif // OPEN_METEO_API_H