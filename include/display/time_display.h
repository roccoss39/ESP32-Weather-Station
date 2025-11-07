#ifndef TIME_DISPLAY_H
#define TIME_DISPLAY_H

#include <TFT_eSPI.h>
#include <time.h>

// --- FUNKCJE WYŚWIETLANIA CZASU ---
void displayTime(TFT_eSPI& tft);
String getPolishDayName(int dayNum);

// --- NOWY OOP CACHE SYSTEM ---
// Zastąpiono 4 extern variables TimeDisplayCache class
// Forward declaration zamiast include w header
class TimeDisplayCache;

// Singleton instance TimeDisplayCache
TimeDisplayCache& getTimeDisplayCache();

#endif