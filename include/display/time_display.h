#ifndef TIME_DISPLAY_H
#define TIME_DISPLAY_H

#include <TFT_eSPI.h>
#include <time.h>

// --- FUNKCJE WYŚWIETLANIA CZASU ---
void displayTime(TFT_eSPI& tft);
String getPolishDayName(int dayNum);

// Zmienne czasowe
extern char timeStrPrev[9];
extern char dateStrPrev[11];

#endif