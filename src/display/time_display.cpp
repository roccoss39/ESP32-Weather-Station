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
  
  int currentWifiStatus = WiFi.status();

  // Sprawdź co się zmieniło i rysuj tylko potrzebne elementy
  
  // 1. CZAS (zmienia się co sekundę) - OOP style
  if (getTimeDisplayCache().hasTimeChanged(timeStr)) {
    Serial.println("Time changed - redrawing time");
    // Wyczyść tylko obszar czasu (przesunięty o 5px w lewo)
    tft.fillRect(TIME_AREA_X + 45, TIME_AREA_Y, 110, 20, COLOR_BACKGROUND);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
    tft.drawString(timeStr, TIME_AREA_X + 45, TIME_AREA_Y);
    
    getTimeDisplayCache().setPrevTimeStr(timeStr);
  }

  // 2. DATA (zmienia się raz dziennie) - OOP style
  if (getTimeDisplayCache().hasDateChanged(dateStr)) {
    Serial.println("Date changed - redrawing date");
    // Wyczyść tylko obszar daty (przesunięty o 5px w lewo)
    tft.fillRect(TIME_AREA_X + 165, TIME_AREA_Y, 120, 20, COLOR_BACKGROUND);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    tft.setTextColor(COLOR_DATE, COLOR_BACKGROUND);
    tft.drawString(dateStr, TIME_AREA_X + 165, TIME_AREA_Y);
    
    getTimeDisplayCache().setPrevDateStr(dateStr);
  }

  // 3. DZIEŃ TYGODNIA (zmienia się raz dziennie) - OOP style
  if (getTimeDisplayCache().hasDayChanged(polishDay)) {
    Serial.println("Day changed - redrawing day");
    // Wyczyść tylko obszar dnia (przesunięty o 5px w lewo)
    tft.fillRect(TIME_AREA_X + 45, TIME_AREA_Y + TIME_AREA_OFFSET_Y, 140, 20, COLOR_BACKGROUND);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    tft.setTextColor(COLOR_DAY, COLOR_BACKGROUND);
    tft.drawString(polishDay, TIME_AREA_X + 45, TIME_AREA_Y + TIME_AREA_OFFSET_Y);
    
    getTimeDisplayCache().setPrevDayStr(polishDay);
  }

  // 4. STATUS WiFi (zmienia się przy problemach z połączeniem) - OOP style
  if (getTimeDisplayCache().hasWifiStatusChanged(currentWifiStatus)) {
    Serial.println("WiFi status changed - redrawing WiFi");
    // Wyczyść tylko obszar WiFi (przesunięty o 5px w lewo)
    tft.fillRect(TIME_AREA_X + 185, TIME_AREA_Y + TIME_AREA_OFFSET_Y, 100, 20, COLOR_BACKGROUND);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    
    if (currentWifiStatus == WL_CONNECTED) {
      tft.setTextColor(COLOR_WIFI_OK, COLOR_BACKGROUND);
      tft.drawString("WiFi: OK", TIME_AREA_X + 185, TIME_AREA_Y + TIME_AREA_OFFSET_Y);
    } else {
      tft.setTextColor(COLOR_WIFI_ERROR, COLOR_BACKGROUND);
      tft.drawString("WiFi: BLAD", TIME_AREA_X + 185, TIME_AREA_Y + TIME_AREA_OFFSET_Y);
    }
    
    getTimeDisplayCache().setPrevWifiStatus(currentWifiStatus);
  }
}