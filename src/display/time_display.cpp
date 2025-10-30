#include "display/time_display.h"
#include "config/display_config.h"
#include <WiFi.h>

// Definicje globalnych zmiennych
char timeStrPrev[9] = "        ";
char dateStrPrev[11] = "          ";

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

  // Rysuj tylko jeśli czas się zmienił
  if (strcmp(timeStr, timeStrPrev) != 0) {
    // Wyczyść obszar czasu
    tft.fillRect(TIME_AREA_X, TIME_AREA_Y, TIME_AREA_WIDTH, TIME_AREA_HEIGHT, COLOR_BACKGROUND);

    // Ustawienia tekstu
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(FONT_SIZE_MEDIUM);
    
    // Czas po lewej
    tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
    tft.drawString(timeStr, TIME_AREA_X + 5, TIME_AREA_Y + 5);
    
    // Data obok czasu
    tft.setTextColor(COLOR_DATE, COLOR_BACKGROUND);
    tft.drawString(dateStr, TIME_AREA_X + 125, TIME_AREA_Y + 5);
    
    // Dzień tygodnia po polsku - w drugim wierszu
    char dayStr[20];
    strftime(dayStr, sizeof(dayStr), "%w", &timeinfo);
    int dayNum = atoi(dayStr);
    String polishDay = getPolishDayName(dayNum);
    
    tft.setTextColor(COLOR_DAY, COLOR_BACKGROUND);
    tft.drawString(polishDay, TIME_AREA_X + 5, TIME_AREA_Y + 30);
    
    // Status WiFi w tej samej linii po prawej
    if (WiFi.status() == WL_CONNECTED) {
      tft.setTextColor(COLOR_WIFI_OK, COLOR_BACKGROUND);
      tft.drawString("WiFi: OK", TIME_AREA_X + 145, TIME_AREA_Y + 30);
    } else {
      tft.setTextColor(COLOR_WIFI_ERROR, COLOR_BACKGROUND);
      tft.drawString("WiFi: BLAD", TIME_AREA_X + 145, TIME_AREA_Y + 30);
    }

    strcpy(timeStrPrev, timeStr);
    strcpy(dateStrPrev, dateStr);

    Serial.print("Time: ");
    Serial.print(dateStr);
    Serial.print(" ");
    Serial.println(timeStr);
  }
}