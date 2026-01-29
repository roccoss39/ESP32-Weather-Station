#include "display/time_display.h"
#include "config/display_config.h"
#include "managers/TimeDisplayCache.h"
#include <WiFi.h>

// Singleton instance TimeDisplayCache  
static TimeDisplayCache timeDisplayCache;

TimeDisplayCache& getTimeDisplayCache() {
  return timeDisplayCache;
}

// ❌ USUNIĘTE: 4 extern variables zastąpione TimeDisplayCache class

String getPolishDayName(int dayNum) {
  switch(dayNum) {
    case 0: return "Niedziela";
    case 1: return "Poniedzialek";
    case 2: return "Wtorek";
    case 3: return "Sroda";
    case 4: return "Czwartek";
    case 5: return "Piatek";
    case 6: return "Sobota";
    default: return "Nieznany";
  }
}

void displayTime(TFT_eSPI& tft) {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 200)) {
    Serial.println("Error reading time");
    tft.fillRect(TIME_AREA_X + 25, TIME_AREA_Y, 140, 20, COLOR_BACKGROUND);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    tft.setTextColor(TFT_YELLOW, COLOR_BACKGROUND);
    tft.drawString("--:--:--", (tft.width()) / 2, TIME_AREA_Y + TIME_AREA_OFFSET_Y + 12);
    return;
  }

  // Formatuj czas
  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  // Formatuj datę
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);

  // Przygotuj wszystkie elementy
  char dayStr[20];
  strftime(dayStr, sizeof(dayStr), "%w", &timeinfo);
  uint8_t dayNum = atoi(dayStr);
  String polishDay = getPolishDayName(dayNum);
  

  // 1. DZIEŃ TYGODNIA (przesunięty o kolejne 10px w lewo)
  if (getTimeDisplayCache().hasDayChanged(polishDay)) {
    Serial.println("Day changed - redrawing day");
    // Wyczyść tylko obszar dnia (przesunięty o 25px w lewo - było +35, teraz +25)
    tft.fillRect(TIME_AREA_X + 25, TIME_AREA_Y, 140, 20, COLOR_BACKGROUND);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    tft.setTextColor(COLOR_DAY, COLOR_BACKGROUND);
    tft.drawString(polishDay, TIME_AREA_X + 25, TIME_AREA_Y);
    
    getTimeDisplayCache().setPrevDayStr(polishDay);
  }

  // 2. DATA (przesunięta o 10px w lewo)
  if (getTimeDisplayCache().hasDateChanged(dateStr)) {
    Serial.println("Date changed - redrawing date");
    // Wyczyść tylko obszar daty (przesunięty o 15px w lewo - było +185, teraz +175)
    tft.fillRect(TIME_AREA_X + 175, TIME_AREA_Y, 120, 20, COLOR_BACKGROUND);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    tft.setTextColor(COLOR_DATE, COLOR_BACKGROUND);
    tft.drawString(dateStr, TIME_AREA_X + 175, TIME_AREA_Y);
    
    getTimeDisplayCache().setPrevDateStr(dateStr);
  }

  // 3. CZAS (minimalny redraw: aktualizuj tylko zmienione cyfry, żeby nie migało)
  if (getTimeDisplayCache().hasTimeChanged(timeStr)) {
    //Serial.println("[DEBUG] Time changed - redrawing time");

    const uint8_t size = FONT_SIZE_LARGE;
    const int charW = 6 * size;   // font 1 is 6px wide
    const int charH = 8 * size;   // font 1 is 8px high

    // "HH:MM:SS" length = 8, fixed width with font 1
    const int totalW = 8 * charW;
    const int x0 = 160 - (totalW / 2);
    const int y0 = TIME_AREA_Y + TIME_AREA_OFFSET_Y; // top-left of time area

    // On first draw (no previous value), draw everything
    const String prev = getTimeDisplayCache().getPrevTimeStr();
    const bool firstDraw = (prev.length() != 8);

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(size);
    tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);

    auto clearChars = [&](int startIndex, int count) {
      tft.fillRect(x0 + startIndex * charW, y0, count * charW, charH, COLOR_BACKGROUND);
    };
    auto drawChars = [&](int startIndex, const char* s, int count) {
      char buf[4] = {0};
      for (int i = 0; i < count; i++) buf[i] = s[i];
      tft.drawString(String(buf), x0 + startIndex * charW, y0);
    };

    if (firstDraw) {
      // Clear full time area once to avoid leftovers from other screens
      tft.fillRect(x0, y0, totalW, charH, COLOR_BACKGROUND);
      tft.drawString(timeStr, x0, y0);
    } else {
      // Hours (0-1)
      if (prev[0] != timeStr[0] || prev[1] != timeStr[1]) {
        clearChars(0, 2);
        drawChars(0, timeStr, 2);
      }
      // Minutes (3-4)
      if (prev[3] != timeStr[3] || prev[4] != timeStr[4]) {
        clearChars(3, 2);
        drawChars(3, timeStr + 3, 2);
      }
      // Seconds (6-7) — changes every second
      if (prev[6] != timeStr[6] || prev[7] != timeStr[7]) {
        clearChars(6, 2);
        drawChars(6, timeStr + 6, 2);
      }
      // Colons (2 and 5) rarely need redraw; draw once if they were missing
      if (prev[2] != ':' || prev[5] != ':') {
        clearChars(2, 1);
        clearChars(5, 1);
        drawChars(2, timeStr + 2, 1);
        drawChars(5, timeStr + 5, 1);
      }
    }

    getTimeDisplayCache().setPrevTimeStr(timeStr);
  }

}