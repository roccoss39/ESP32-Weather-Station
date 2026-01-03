#include "display/sensors_display.h"
#include "config/display_config.h"
#include "sensors/dht22_sensor.h"
#include "weather/forecast_data.h" // Potrzebne do weeklyForecast
#include "config/hardware_config.h" // Dla FIRMWARE_VERSION
#include <WiFi.h>

// --- ZMIENNE GLOBALNE POTRZEBNE DO STOPKI SYSTEMOWEJ ---
extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;
extern bool isOfflineMode;

// --- FALLBACK CONSTANTS ---
// Definiujemy stałe pozycji stopki, jeśli nie zostały zdefiniowane w configu
#ifndef UPDATES_TITLE_Y
  #define UPDATES_CLEAR_Y   130
  #define UPDATES_TITLE_Y   140
  #define UPDATES_DHT22_Y   160
  #define UPDATES_SENSOR_Y  175
  #define UPDATES_WEATHER_Y 190
  #define UPDATES_WEEKLY_Y  205
  #define UPDATES_WIFI_Y    220
#endif

// === FUNKCJA POMOCNICZA DO PASKÓW POSTĘPU ===
// Używana lokalnie tylko w tym pliku
static void drawProgressBar(TFT_eSPI& tft, int x, int y, int w, int h, float val, float minVal, float maxVal, uint16_t color) {
    tft.drawRoundRect(x, y, w, h, 4, TFT_DARKGREY);
    
    float range = maxVal - minVal;
    if (range == 0) range = 1; // Zabezpieczenie przez dzieleniem przez 0
    
    float percent = (val - minVal) / range;
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    
    int fillW = (int)((w - 4) * percent);
    
    // Tło paska (ciemny szary/grafit)
    tft.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 2, 0x10A2); 
    
    // Wypełnienie
    if (fillW > 0) {
        tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, color);
    }
}

// === GŁÓWNA FUNKCJA WYŚWIETLANIA SENSORÓW ===
void displayLocalSensors(TFT_eSPI& tft) {
  Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS (PRO)");
  
  tft.fillScreen(COLOR_BACKGROUND);

  // Pobierz dane
  DHT22Data dhtData = getDHT22Data();
  float temp = dhtData.temperature;
  float hum = dhtData.humidity;
  bool isValid = dhtData.isValid;

  // === KONFIGURACJA UKŁADU ===
  bool isCompactMode = !isOfflineMode;  // Online = kompaktowy, Offline = pełny (2 karty)
  
  // Ustawienia geometryczne
  uint8_t cardY = 55;       // Zaczynamy zaraz pod nagłówkiem (linia jest na 45)
  int cardH;
  
  if (isCompactMode) {
      cardH = 70;       // Online: kończy na Y=125 (stopka od 130)
  } else {
      cardH = 100;      // Offline: zmniejszone z 140 (miejsce na dużą datę Y=165)
  }
  
  uint8_t cardW = 135;      
  uint8_t card1_X = 20;     
  uint8_t card2_X = 165;    

  tft.drawFastHLine(20, 45, 280, TFT_DARKGREY); 
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("WARUNKI W POMIESZCZENIU", tft.width() / 2, 35);


  // =========================================================
  // KARTA 1: TEMPERATURA
  // =========================================================
  tft.fillRoundRect(card1_X, cardY, cardW, cardH, 8, 0x1082); 
  tft.drawRoundRect(card1_X, cardY, cardW, cardH, 8, TFT_DARKGREY); 

  // Etykieta
  tft.setTextColor(TFT_ORANGE, 0x1082);
  tft.setTextDatum(MC_DATUM); 
  tft.setTextSize(1);
  uint8_t labelY = cardY + 15; // Stała pozycja etykiety
  tft.drawString("TEMP", card1_X + cardW/2, labelY);
  
  if (isValid) {
      uint16_t tempColor = TFT_GREEN;
      if (temp < 18) tempColor = TFT_CYAN;     
      if (temp > 24) tempColor = TFT_ORANGE;    
      if (temp > 28) tempColor = TFT_RED;       
      
      tft.setTextColor(tempColor, 0x1082);
      
      // Pozycjonowanie wartości (zależne od wysokości karty)
      int valY = isCompactMode ? cardY + 40 : cardY + 60;
      
      tft.setTextFont(4); 
      tft.drawString(String(temp, 1), card1_X + cardW/2, valY);
      
      // Jednostka
      tft.setTextFont(2); 
      if (isCompactMode) {
          // W trybie kompaktowym jednostka obok liczby
          tft.drawString("'C", card1_X + cardW/2 + 45, valY - 5);
      } else {
          // W trybie dużym pod spodem
          tft.drawString("st C", card1_X + cardW/2, cardY + 90);
      }
      
      // Pasek postępu
      if (!isCompactMode) {
          drawProgressBar(tft, card1_X + 15, cardY + 110, cardW - 30, 10, temp, 0, 40, tempColor);
      } else {
          // Wersja mini na dole karty
          drawProgressBar(tft, card1_X + 10, cardY + cardH - 8, cardW - 20, 4, temp, 0, 40, tempColor);
      }
      
  } else {
      tft.setTextColor(TFT_RED, 0x1082);
      tft.setTextFont(4); 
      tft.drawString("--.-", card1_X + cardW/2, cardY + cardH/2);
  }


  // =========================================================
  // KARTA 2: WILGOTNOŚĆ
  // =========================================================
  tft.fillRoundRect(card2_X, cardY, cardW, cardH, 8, 0x1082);
  tft.drawRoundRect(card2_X, cardY, cardW, cardH, 8, TFT_DARKGREY);

  tft.setTextColor(TFT_CYAN, 0x1082);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextDatum(MC_DATUM);
  tft.drawString("WILGOTNOSC", card2_X + cardW/2, labelY);
  
  if (isValid) {
      uint16_t humColor = TFT_GREEN;
      if (hum < 30) humColor = TFT_YELLOW;  
      if (hum > 60) humColor = TFT_BLUE;    
      
      tft.setTextColor(humColor, 0x1082);
      
      int valY = isCompactMode ? cardY + 35 : cardY + 60;

      tft.setTextFont(4);
      tft.drawString(String((int)hum), card2_X + cardW/2, valY);
      
      tft.setTextFont(2);
      if (isCompactMode) {
          tft.drawString("%", card2_X + cardW/2 + 35, valY - 5);
      } else {
          tft.drawString("% RH", card2_X + cardW/2, cardY + 90);
      }
      
      if (!isCompactMode) {
          drawProgressBar(tft, card2_X + 15, cardY + 110, cardW - 30, 10, hum, 0, 100, humColor);
      } else {
          drawProgressBar(tft, card2_X + 10, cardY + cardH - 8, cardW - 20, 4, hum, 0, 100, humColor);
      }
      
  } else {
      tft.setTextColor(TFT_RED, 0x1082);
      tft.setTextFont(4); 
      tft.drawString("--", card2_X + cardW/2, cardY + cardH/2);
  }
  
  
  // =========================================================
  // FOOTER (STOPKA SYSTEMOWA) - TYLKO W TRYBIE ONLINE
  // =========================================================
  if (!isOfflineMode) {
      
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextFont(1);
      
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      
      // Czyścimy obszar stopki
      tft.fillRect(0, UPDATES_CLEAR_Y, 320, 240 - UPDATES_CLEAR_Y, COLOR_BACKGROUND);
      
      // Etykieta
      tft.setTextDatum(TC_DATUM);
      tft.setTextColor(TFT_CYAN);
      tft.drawString("STATUS SYSTEMU:", 160, UPDATES_TITLE_Y);
      
      // 1. Status DHT
      String dhtStatus = "DHT22: " + dhtData.status;
      uint16_t statusColor = dhtData.isValid ? TFT_GREEN : TFT_RED;
      tft.setTextColor(statusColor);
      tft.drawString(dhtStatus, 160, UPDATES_DHT22_Y);
      
      // 2. Info o sensorze
      tft.setTextColor(TFT_DARKGREY);
      tft.drawString("Odczyt sensora: co 2s", 160, UPDATES_SENSOR_Y);
      
      // 3. Pogoda bieżąca (OSOBNA LINIA)
      unsigned long weatherAge = (millis() - lastWeatherCheckGlobal) / 1000;
      String wPrefix = "Pogoda: ";
      String wTime;
      if (weatherAge < 60) wTime = String(weatherAge) + "s temu";
      else if (weatherAge < 3600) wTime = String(weatherAge / 60) + "min temu";
      else wTime = String(weatherAge / 3600) + "h temu";
      String wSuffix = " (co 10min)";
      
      int wTotalWidth = tft.textWidth(wPrefix + wTime + wSuffix);
      int wX = 160 - (wTotalWidth / 2);
      
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(TFT_DARKGREY); tft.drawString(wPrefix, wX, UPDATES_WEATHER_Y);
      wX += tft.textWidth(wPrefix);
      tft.setTextColor(TFT_WHITE);    tft.drawString(wTime, wX, UPDATES_WEATHER_Y);
      wX += tft.textWidth(wTime);
      tft.setTextColor(TFT_DARKGREY); tft.drawString(wSuffix, wX, UPDATES_WEATHER_Y);
      
      // 4. Prognoza weekly (OSOBNA LINIA)
      unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
      String fPrefix = "Pogoda tyg.: ";
      String fTime;
      if (weeklyAge < 60) fTime = String(weeklyAge) + "s temu";
      else if (weeklyAge < 3600) fTime = String(weeklyAge / 60) + "min temu";
      else fTime = String(weeklyAge / 3600) + "h temu";
      String fSuffix = " (co 4h)";
      
      int fTotalWidth = tft.textWidth(fPrefix + fTime + fSuffix);
      int fX = 160 - (fTotalWidth / 2);
      
      tft.setTextColor(TFT_DARKGREY); tft.drawString(fPrefix, fX, UPDATES_WEEKLY_Y);
      fX += tft.textWidth(fPrefix);
      tft.setTextColor(TFT_WHITE);    tft.drawString(fTime, fX, UPDATES_WEEKLY_Y);
      fX += tft.textWidth(fTime);
      tft.setTextColor(TFT_DARKGREY); tft.drawString(fSuffix, fX, UPDATES_WEEKLY_Y);
      
      // 5. Stan WiFi
      String wifiStatus;
      if (WiFi.status() == WL_CONNECTED) {
        int8_t rssi = WiFi.RSSI();
        uint8_t quality = 0;
        if(rssi <= -100) quality = 0;
        else if(rssi >= -50) quality = 100;
        else quality = 2 * (rssi + 100);
        wifiStatus = "WiFi: " + String(WiFi.SSID()) + " (" + String(quality) + "%)";
      } else {
        tft.setTextColor(TFT_RED);
        wifiStatus = "WiFi: Rozlaczony";
      }
      
      tft.setTextDatum(TC_DATUM);
      tft.setTextColor(TFT_DARKGREY);
      tft.drawString(wifiStatus, 160, UPDATES_WIFI_Y);

      // Stopka wersji (Prawy Dolny Róg)
      tft.setTextDatum(BR_DATUM); 
      tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND); 
      String versionText = "v" + String(FIRMWARE_VERSION);
      tft.drawString(versionText, tft.width() - 5, tft.height() - 5);
  }
  
  // Reset ustawień
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
}