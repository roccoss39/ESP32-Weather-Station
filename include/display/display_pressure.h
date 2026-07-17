#ifndef DISPLAY_PRESSURE_H
#define DISPLAY_PRESSURE_H

#include <TFT_eSPI.h> // Niezbędne do przekazania ekranu w argumencie

// Flaga wyboru wysokości ciśnienia: true = MSL (nad poziomem morza), false = na poziomie gruntu (stacji)
extern bool showPressureAtSeaLevel;

// Funkcja odpowiedzialna za rysowanie wykresu i interfejsu ciśnienia
void displayPressureScreen(TFT_eSPI& tft);

#endif // DISPLAY_PRESSURE_H