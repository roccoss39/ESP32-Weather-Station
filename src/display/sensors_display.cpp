#include "display/sensors_display.h"
#include "config/display_config.h"
#include "weather/forecast_data.h"
#include "config/hardware_config.h"
#include <WiFi.h>
#include "display/time_display.h" 

#ifdef USE_SHT31
  #include "sensors/sht31_sensor.h"
#else
  #include "sensors/dht22_sensor.h"
#endif

extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;
extern bool isOfflineMode;

#define CARD_BG_COLOR 0x1082 

// STAŁE POZYCJI DLA STOPKI ONLINE (Metoda Fixed Slots)
#define COL_PREFIX_X  115  
#define COL_CENTER_X  160  
#define COL_SUFFIX_X  205  

#ifndef UPDATES_TITLE_Y
  #define UPDATES_CLEAR_Y   130
  #define UPDATES_TITLE_Y   140
  #define UPDATES_DHT22_Y   160  
  #define UPDATES_SENSOR_Y  175  
  #define UPDATES_WEATHER_Y 190
  #define UPDATES_WEEKLY_Y  205
  #define UPDATES_WIFI_Y    220
#endif

static void drawProgressBar(TFT_eSPI& tft, int x, int y, int w, int h, float val, float minVal, float maxVal, uint16_t color) {
    float range = maxVal - minVal;
    if (range == 0) range = 1;
    float percent = (val - minVal) / range;
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    int fillW = (int)((w - 4) * percent);
    tft.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 2, 0x10A2);
    if (fillW > 0) tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, color);
}

void displayLocalSensors(TFT_eSPI& tft, bool onlyUpdate) {
  
  // 1. ZABEZPIECZENIE CZCIONKI
  tft.setTextSize(1);

  // 2. TŁO (Tylko raz przy zmianie ekranu)
  if (!onlyUpdate) {
      Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS");
      tft.fillScreen(COLOR_BACKGROUND);
  }

  // 3. ZEGAR (Zawsze odświeżamy)
  // W trybie Online stopka jest na dole, więc zegar nie przeszkadza (ale go tam nie wyświetlamy w stopce).
  // W trybie Offline zegar jest wyświetlany.
  // Funkcja displayTime() sama decyduje gdzie rysować, ale tutaj wywołujemy ją, 
  // by wewnętrzny licznik czasu się aktualizował.
  // (Jeśli w trybie Online nie chcesz zegara na górze, przykryje go nagłówek "WARUNKI...")
  if (isOfflineMode) {
      displayTime(tft);
  }
  // W trybie Online nie wywołujemy displayTime na ekranie sensorów, 
  // bo mamy tam nagłówek.

  // 4. RESETOWANIE USTAWIEŃ PO ZEGARZE
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND); 

  // =========================================================
  // 5. DEKLARACJA I POBIERANIE DANYCH (KLUCZOWE MIEJSCE)
  // =========================================================
  // Te zmienne MUSZĄ być zadeklarowane TUTAJ, przed if/else
  float temp = 0.0;
  float hum = 0.0;
  bool isValid = false;
  
  // Zmienne do stopki (To ich brakowało!)
  String sensorName = "";
  String sensorStatusMsg = "";
  int readIntervalSec = 0;
  
  int tempDecimals = 1; 
  bool humIsInt = true;

  #ifdef USE_SHT31
    temp = localTemperature;
    hum = localHumidity;
    isValid = (hum != 0.0 && !isnan(temp)); 
    
    // Ustawiamy zmienne opisowe
    sensorName = "SHT31";
    sensorStatusMsg = isValid ? "OK" : "BLAD";
    readIntervalSec = 1;
    
    tempDecimals = 2; 
    humIsInt = false; 
  #else
    DHT22Data dhtData = getDHT22Data();
    temp = dhtData.temperature;
    hum = dhtData.humidity;
    isValid = dhtData.isValid;
    
    // Ustawiamy zmienne opisowe
    sensorName = "DHT22";
    sensorStatusMsg = dhtData.status;
    readIntervalSec = (DHT22_READ_INTERVAL / 1000);
    
    tempDecimals = 1;
    humIsInt = true;
  #endif

  // ##################################################################
  // IF: TRYB OFFLINE (DUŻE KARTY + ZEGAR + BRAK STOPKI)
  // ##################################################################
  if (isOfflineMode) {
    
    uint8_t cardStartY = 70;   
    uint8_t cardH = 95;        
    uint8_t cardW = 145;       
    uint8_t card1_X = 10;
    uint8_t card2_X = 165;

    // --- RYSOWANIE STATYCZNE OFFLINE ---
    if (!onlyUpdate) {
        uint8_t headerY = 55;
        tft.drawFastHLine(0, headerY, 320, TFT_DARKGREY); 
        tft.setTextColor(TFT_SILVER, TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WARUNKI W POMIESZCZENIU", 160, headerY - 10);

        tft.fillRoundRect(card1_X, cardStartY, cardW, cardH, 6, CARD_BG_COLOR);
        tft.fillRoundRect(card2_X, cardStartY, cardW, cardH, 6, CARD_BG_COLOR);
        tft.drawRoundRect(card1_X, cardStartY, cardW, cardH, 6, TFT_DARKGREY);
        tft.drawRoundRect(card2_X, cardStartY, cardW, cardH, 6, TFT_DARKGREY);

        tft.setTextColor(TFT_ORANGE, CARD_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(1);
        tft.drawString("TEMP", card1_X + cardW/2, cardStartY + 10);

        tft.setTextColor(TFT_CYAN, CARD_BG_COLOR);
        tft.drawString("WILGOTNOSC", card2_X + cardW/2, cardStartY + 10);

        int valY = cardStartY + 48;
        tft.setTextColor(isValid ? TFT_GREEN : TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(2);
        tft.setTextDatum(BL_DATUM);
        tft.drawString("'C", card1_X + cardW/2 + 35, valY + 8); 
        tft.drawString("%", card2_X + cardW/2 + 25, valY + 8);
        
        tft.drawRoundRect(card1_X + 8, cardStartY + cardH - 25, cardW - 16, 6, 3, TFT_DARKGREY);
        tft.drawRoundRect(card2_X + 8, cardStartY + cardH - 25, cardW - 16, 6, 3, TFT_DARKGREY);
    }

    // --- RYSOWANIE DYNAMICZNE OFFLINE ---
    tft.setTextSize(1);
    int valY = cardStartY + 48;
    
    // Temp
    if (isValid) {
        uint16_t tempColor = TFT_GREEN;
        if (temp < 18) tempColor = TFT_CYAN;
        if (temp > 24) tempColor = TFT_ORANGE;
        if (temp > 28) tempColor = TFT_RED;
        tft.setTextColor(tempColor, CARD_BG_COLOR); 
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(temp, tempDecimals), card1_X + cardW/2 - 5, valY);
        drawProgressBar(tft, card1_X + 8, cardStartY + cardH - 25, cardW - 16, 6, temp, 0, 40, tempColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--.-", card1_X + cardW/2, cardStartY + 48);
    }

    // Wilg
    if (isValid) {
        uint16_t humColor = TFT_GREEN;
        if (hum < 30) humColor = TFT_YELLOW;
        if (hum > 60) humColor = TFT_BLUE;
        tft.setTextColor(humColor, CARD_BG_COLOR); 
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        if (humIsInt) tft.drawString(String((int)hum), card2_X + cardW/2 - 5, valY);
        else tft.drawString(String(hum, 2), card2_X + cardW/2 - 5, valY);
        drawProgressBar(tft, card2_X + 8, cardStartY + cardH - 25, cardW - 16, 6, hum, 0, 100, humColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--", card2_X + cardW/2, cardStartY + 48);
    }
  } 
  // ##################################################################
  // ELSE: TRYB ONLINE (KOMPAKTOWE KARTY + STOPKA + BRAK ZEGARA)
  // ##################################################################
  else {
    
    uint8_t cardY = 55;
    int cardH = 70;
    uint8_t cardW = 135;
    uint8_t card1_X = 20;
    uint8_t card2_X = 165;
    
    // --- RYSOWANIE STATYCZNE ONLINE ---
    if (!onlyUpdate) {
        uint8_t headerY = 45;
        tft.drawFastHLine(20, headerY, 280, TFT_DARKGREY);
        tft.setTextColor(TFT_SILVER, TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WARUNKI W POMIESZCZENIU", tft.width() / 2, 35);
        
        tft.fillRoundRect(card1_X, cardY, cardW, cardH, 8, CARD_BG_COLOR);
        tft.fillRoundRect(card2_X, cardY, cardW, cardH, 8, CARD_BG_COLOR);
        tft.drawRoundRect(card1_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
        tft.drawRoundRect(card2_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
        
        tft.setTextColor(TFT_ORANGE, CARD_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(1);
        tft.drawString("TEMP", card1_X + cardW/2, cardY + 15);
        
        tft.setTextColor(TFT_CYAN, CARD_BG_COLOR);
        tft.drawString("WILGOTNOSC", card2_X + cardW/2, cardY + 15);
        
        int valY = cardY + 40;
        tft.setTextColor(TFT_GREEN, CARD_BG_COLOR);
        tft.setTextFont(2);
        tft.drawString("'C", card1_X + cardW/2 + 50, valY - 5);
        tft.drawString("%", card2_X + cardW/2 + 45, valY - 5);
        
        tft.drawRoundRect(card1_X + 10, cardY + cardH - 8, cardW - 20, 4, 2, TFT_DARKGREY);
        tft.drawRoundRect(card2_X + 10, cardY + cardH - 8, cardW - 20, 4, 2, TFT_DARKGREY);
        
        // --- STATYCZNA STOPKA (BEZ ZMIENNYCH DANYCH) ---
        tft.fillRect(0, UPDATES_CLEAR_Y, 320, 240 - UPDATES_CLEAR_Y, COLOR_BACKGROUND);
        tft.setTextFont(1);
        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_CYAN, COLOR_BACKGROUND);
        tft.drawString("STATUS SYSTEMU:", 160, UPDATES_TITLE_Y);

        // ETYKIETY POGODY (STAŁE - ZERO MIGANIA)
        tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
        
        tft.setTextDatum(TR_DATUM);
        tft.drawString("Pogoda: ", COL_PREFIX_X, UPDATES_WEATHER_Y);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(" (co 10min)", COL_SUFFIX_X, UPDATES_WEATHER_Y);

        tft.setTextDatum(TR_DATUM);
        tft.drawString("Pogoda tyg.: ", COL_PREFIX_X, UPDATES_WEEKLY_Y);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(" (co 4h)", COL_SUFFIX_X, UPDATES_WEEKLY_Y);
    }
    
    // --- RYSOWANIE DYNAMICZNE ONLINE (LICZBY) ---
    tft.setTextSize(1);
    int valY = cardY + 40;
    
    // Temp
    if (isValid) {
        uint16_t tempColor = TFT_GREEN;
        if (temp < 18) tempColor = TFT_CYAN;
        if (temp > 24) tempColor = TFT_ORANGE;
        if (temp > 28) tempColor = TFT_RED;
        tft.setTextColor(tempColor, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(temp, tempDecimals), card1_X + cardW/2, valY);
        drawProgressBar(tft, card1_X + 10, cardY + cardH - 8, cardW - 20, 4, temp, 0, 40, tempColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.drawString("--.-", card1_X + cardW/2, cardY + cardH/2);
    }
    
    // Wilg
    if (isValid) {
        uint16_t humColor = TFT_GREEN;
        if (hum < 30) humColor = TFT_YELLOW;
        if (hum > 60) humColor = TFT_BLUE;
        tft.setTextColor(humColor, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        if (humIsInt) tft.drawString(String((int)hum), card2_X + cardW/2, valY);
        else tft.drawString(String(hum, 2), card2_X + cardW/2, valY);
        drawProgressBar(tft, card2_X + 10, cardY + cardH - 8, cardW - 20, 4, hum, 0, 100, humColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.drawString("--", card2_X + cardW/2, cardY + cardH/2);
    }

    // --- RYSOWANIE DYNAMICZNE ONLINE (STOPKA) ---
    // Tutaj używamy zmiennych sensorName, sensorStatusMsg, readIntervalSec
    // Ponieważ są zadeklarowane na górze, błąd kompilacji zniknie.
    
    tft.setTextFont(1);
    tft.setTextSize(1);
    
    tft.setTextPadding(200); 

    tft.setTextDatum(TC_DATUM);
    String line = (sensorName == "" ? "SHT31" : sensorName) + ": " + sensorStatusMsg;
    uint16_t statusColor = isValid ? TFT_GREEN : TFT_RED;
    tft.setTextColor(statusColor, COLOR_BACKGROUND);
    tft.drawString(line, 160, UPDATES_DHT22_Y); 
    
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    String sensorInterval = "Odczyt sensora: co " + String(readIntervalSec) + "s";
    tft.drawString(sensorInterval, 160, UPDATES_SENSOR_Y);
    
    // CZAS POGODY (SLOT)
    tft.setTextPadding(80); 
    
    unsigned long weatherAge = (millis() - lastWeatherCheckGlobal) / 1000;
    String wTime;
    if (weatherAge < 60) wTime = String(weatherAge) + "s temu";
    else if (weatherAge < 3600) wTime = String(weatherAge / 60) + "m temu";
    else wTime = String(weatherAge / 3600) + "h temu";
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.drawString(wTime, COL_CENTER_X, UPDATES_WEATHER_Y);

    // CZAS WEEKLY (SLOT)
    unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
    String fTime;
    if (weeklyAge < 60) fTime = String(weeklyAge) + "s temu";
    else if (weeklyAge < 3600) fTime = String(weeklyAge / 60) + "m temu";
    else fTime = String(weeklyAge / 3600) + "h temu";
    
    tft.drawString(fTime, COL_CENTER_X, UPDATES_WEEKLY_Y);
    
    tft.setTextPadding(0);

    // WIFI
    tft.setTextDatum(TC_DATUM);
    tft.setTextPadding(320); 
    String wifiTxt = (WiFi.status() == WL_CONNECTED) ? "WiFi: " + String(WiFi.SSID()) : "WiFi: Rozlaczony";
    uint16_t wifiColor = (WiFi.status() == WL_CONNECTED) ? TFT_DARKGREY : TFT_RED;
    tft.setTextColor(wifiColor, COLOR_BACKGROUND);
    tft.drawString(wifiTxt, 160, UPDATES_WIFI_Y);
    tft.setTextPadding(0);
    
    // VERSION
    tft.setTextDatum(BR_DATUM);
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    tft.drawString("v" + String(FIRMWARE_VERSION), 315, 235);
  }
  
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
}