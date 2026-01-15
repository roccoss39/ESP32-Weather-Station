#ifndef SHT31_SENSOR_H
#define SHT31_SENSOR_H

#include <Arduino.h>

// Zmienne globalne dostępne dla innych plików (np. dla wyświetlacza)
extern float localTemperature;
extern float localHumidity;

// Funkcje
void initSHT31();
void updateSHT31();

#endif