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
  
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error reading time");
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
  int dayNum = atoi(dayStr);
  String polishDay = getPolishDayName(dayNum);
  
  // Usunięto currentWifiStatus - nie potrzebne bez wyświetlania WiFi status

  // Sprawdź co się zmieniło i rysuj tylko potrzebne elementy
  
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

  // 3. CZAS (wyśrodkowany na ekranie - WIĘKSZA czcionka)
  if (getTimeDisplayCache().hasTimeChanged(timeStr)) {
    Serial.println("Time changed - redrawing time");
    // Wyczyść cały obszar czasu (wyśrodkowany)
    tft.fillRect(TIME_AREA_X, TIME_AREA_Y + TIME_AREA_OFFSET_Y, 320, 25, COLOR_BACKGROUND);
    
    tft.setTextDatum(MC_DATUM);  // Middle Center - wyśrodkowanie
    tft.setTextSize(FONT_SIZE_LARGE);  // ZWIĘKSZONA czcionka o 1
    tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
    tft.drawString(timeStr, 160, TIME_AREA_Y + TIME_AREA_OFFSET_Y + 12); // X=160 (środek ekranu 320/2)
    
    getTimeDisplayCache().setPrevTimeStr(timeStr);
  }

  // USUNIĘTO: STATUS WiFi zgodnie z życzeniem
}