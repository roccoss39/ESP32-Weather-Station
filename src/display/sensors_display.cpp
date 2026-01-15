#include "display/sensors_display.h"
#include "config/display_config.h"
#include "weather/forecast_data.h"
#include "config/hardware_config.h"
#include <WiFi.h>

// --- WARUNKOWE IMPORTY CZUJNIKÓW ---
#ifdef USE_SHT31
  #include "sensors/sht31_sensor.h"
#else
  #include "sensors/dht22_sensor.h"
#endif

// --- ZMIENNE GLOBALNE ---
extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;
extern bool isOfflineMode;

// --- STAŁE POZYCJI DLA STOPKI (TRYB ONLINE) ---
// Blok zabezpieczający - używamy nazw, które (prawdopodobnie) masz w display_config.h
// Jeśli ich nie ma, zostaną zdefiniowane tutaj.
#ifndef UPDATES_TITLE_Y
  #define UPDATES_CLEAR_Y   130
  #define UPDATES_TITLE_Y   140
  #define UPDATES_DHT22_Y   160  // Użyjemy tego dla Statusu Sensora
  #define UPDATES_SENSOR_Y  175  // Użyjemy tego dla Interwału
  #define UPDATES_WEATHER_Y 190
  #define UPDATES_WEEKLY_Y  205
  #define UPDATES_WIFI_Y    220
#endif

// === FUNKCJA POMOCNICZA DO PASKÓW POSTĘPU ===
static void drawProgressBar(TFT_eSPI& tft, int x, int y, int w, int h, float val, float minVal, float maxVal, uint16_t color) {
    tft.drawRoundRect(x, y, w, h, 4, TFT_DARKGREY);
    
    float range = maxVal - minVal;
    if (range == 0) range = 1;
    
    float percent = (val - minVal) / range;
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    
    int fillW = (int)((w - 4) * percent);
    
    tft.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 2, 0x10A2);
    if (fillW > 0) {
        tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, color);
    }
}

// === GŁÓWNA FUNKCJA WYŚWIETLANIA SENSORÓW ===
void displayLocalSensors(TFT_eSPI& tft) {
  Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS");
  
  tft.fillScreen(COLOR_BACKGROUND);

  // =========================================================
  // 1. POBIERANIE DANYCH (ZALEŻNIE OD KONFIGURACJI)
  // =========================================================
  float temp = 0.0;
  float hum = 0.0;
  bool isValid = false;
  String sensorName = "";
  String sensorStatusMsg = "";
  int readIntervalSec = 0;
  
  // Zmienne do formatowania tekstu (ile miejsc po przecinku)
  int tempDecimals = 1; 
  bool humIsInt = true;

  #ifdef USE_SHT31
    // --- TRYB SHT31 ---
    temp = localTemperature;
    hum = localHumidity;
    isValid = (hum != 0.0 && !isnan(temp)); 
    sensorName = "SHT31";
    sensorStatusMsg = isValid ? "OK" : "BLAD";
    readIntervalSec = 1;
    
    // SHT31 jest precyzyjny - chcemy 2 miejsca po przecinku
    tempDecimals = 2;
    humIsInt = false; // Chcemy float dla wilgotności
  #else
    // --- TRYB DHT22 ---
    DHT22Data dhtData = getDHT22Data();
    temp = dhtData.temperature;
    hum = dhtData.humidity;
    isValid = dhtData.isValid;
    sensorName = "DHT22";
    sensorStatusMsg = dhtData.status;
    readIntervalSec = (DHT22_READ_INTERVAL / 1000);
    
    // DHT22 - standardowo 1 miejsce temp, 0 miejsc wilgotność
    tempDecimals = 1;
    humIsInt = true;
  #endif

  // =========================================================
  // KONFIGURACJA LAYOUTU - OSOBNO DLA OFFLINE I ONLINE
  // =========================================================
  
  if (isOfflineMode) {
    // ╔══════════════════════════════════════════════════════╗
    // ║  TRYB OFFLINE                                        ║
    // ╚══════════════════════════════════════════════════════╝
    
    uint8_t headerY = 55;
    tft.drawFastHLine(0, headerY, 320, TFT_DARKGREY); 
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WARUNKI W POMIESZCZENIU", 160, headerY - 10);
    
    uint8_t cardStartY = 70;   
    uint8_t cardH = 95;        
    uint8_t cardW = 145;       
    uint8_t card1_X = 10;
    uint8_t card2_X = 165;
    
    // KARTA 1: TEMPERATURA
    tft.fillRoundRect(card1_X, cardStartY, cardW, cardH, 6, 0x1082);
    tft.drawRoundRect(card1_X, cardStartY, cardW, cardH, 6, TFT_DARKGREY);
    
    tft.setTextColor(TFT_ORANGE, 0x1082);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("TEMP", card1_X + cardW/2, cardStartY + 10);
    
    if (isValid) {
        uint16_t tempColor = TFT_GREEN;
        if (temp < 18) tempColor = TFT_CYAN;
        if (temp > 24) tempColor = TFT_ORANGE;
        if (temp > 28) tempColor = TFT_RED;
        
        tft.setTextColor(tempColor, 0x1082);
        int valY = cardStartY + 48;
        
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        // FORMATOWANIE ZALEŻNE OD CZUJNIKA
        tft.drawString(String(temp, tempDecimals), card1_X + cardW/2 - 5, valY);
        
        tft.setTextFont(2);
        tft.setTextDatum(BL_DATUM);
        tft.drawString("'C", card1_X + cardW/2 + 35, valY + 8); // Przesunięte w prawo bo szerszy tekst
        
        drawProgressBar(tft, card1_X + 8, cardStartY + cardH - 25, cardW - 16, 6, temp, 0, 40, tempColor);
    } else {
        tft.setTextColor(TFT_RED, 0x1082);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--.-", card1_X + cardW/2, cardStartY + 48);
    }
    
    // KARTA 2: WILGOTNOŚĆ
    tft.fillRoundRect(card2_X, cardStartY, cardW, cardH, 6, 0x1082);
    tft.drawRoundRect(card2_X, cardStartY, cardW, cardH, 6, TFT_DARKGREY);
    
    tft.setTextColor(TFT_CYAN, 0x1082);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WILGOTNOSC", card2_X + cardW/2, cardStartY + 10);
    
    if (isValid) {
        uint16_t humColor = TFT_GREEN;
        if (hum < 30) humColor = TFT_YELLOW;
        if (hum > 60) humColor = TFT_BLUE;
        
        tft.setTextColor(humColor, 0x1082);
        int valY = cardStartY + 48;
        
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        
        // FORMATOWANIE ZALEŻNE OD CZUJNIKA
        if (humIsInt) {
             tft.drawString(String((int)hum), card2_X + cardW/2 - 5, valY);
        } else {
             tft.drawString(String(hum, 2), card2_X + cardW/2 - 5, valY);
        }
        
        tft.setTextFont(2);
        tft.setTextDatum(BL_DATUM);
        tft.drawString("%", card2_X + cardW/2 + 25, valY + 8);
        
        drawProgressBar(tft, card2_X + 8, cardStartY + cardH - 25, cardW - 16, 6, hum, 0, 100, humColor);
    } else {
        tft.setTextColor(TFT_RED, 0x1082);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--", card2_X + cardW/2, cardStartY + 48);
    }
    
  } else {
    // ╔══════════════════════════════════════════════════════╗
    // ║         TRYB ONLINE                                  ║
    // ╚══════════════════════════════════════════════════════╝
    
    uint8_t headerY = 45;
    tft.drawFastHLine(20, headerY, 280, TFT_DARKGREY);
    tft.setTextColor(TFT_SILVER, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WARUNKI W POMIESZCZENIU", tft.width() / 2, 35);
    
    uint8_t cardY = 55;
    int cardH = 70;
    uint8_t cardW = 135;
    uint8_t card1_X = 20;
    uint8_t card2_X = 165;
    
    // KARTA 1: TEMPERATURA
    tft.fillRoundRect(card1_X, cardY, cardW, cardH, 8, 0x1082);
    tft.drawRoundRect(card1_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
    
    tft.setTextColor(TFT_ORANGE, 0x1082);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.drawString("TEMP", card1_X + cardW/2, cardY + 15);
    
    if (isValid) {
        uint16_t tempColor = TFT_GREEN;
        if (temp < 18) tempColor = TFT_CYAN;
        if (temp > 24) tempColor = TFT_ORANGE;
        if (temp > 28) tempColor = TFT_RED;
        
        tft.setTextColor(tempColor, 0x1082);
        int valY = cardY + 40;
        
        tft.setTextFont(4);
        // Formatowanie 2 miejsc po przecinku dla SHT
        tft.drawString(String(temp, tempDecimals), card1_X + cardW/2, valY);
        
        tft.setTextFont(2);
        tft.drawString("'C", card1_X + cardW/2 + 50, valY - 5); // Odsunięte 'C'
        
        drawProgressBar(tft, card1_X + 10, cardY + cardH - 8, cardW - 20, 4, temp, 0, 40, tempColor);
    } else {
        tft.setTextColor(TFT_RED, 0x1082);
        tft.setTextFont(4);
        tft.drawString("--.-", card1_X + cardW/2, cardY + cardH/2);
    }
    
    // KARTA 2: WILGOTNOŚĆ
    tft.fillRoundRect(card2_X, cardY, cardW, cardH, 8, 0x1082);
    tft.drawRoundRect(card2_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
    
    tft.setTextColor(TFT_CYAN, 0x1082);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WILGOTNOSC", card2_X + cardW/2, cardY + 15);
    
    if (isValid) {
        uint16_t humColor = TFT_GREEN;
        if (hum < 30) humColor = TFT_YELLOW;
        if (hum > 60) humColor = TFT_BLUE;
        
        tft.setTextColor(humColor, 0x1082);
        int valY = cardY + 40;
        
        tft.setTextFont(4);
        
        // Formatowanie 2 miejsc po przecinku dla SHT
        if (humIsInt) {
             tft.drawString(String((int)hum), card2_X + cardW/2, valY);
        } else {
             tft.drawString(String(hum, 2), card2_X + cardW/2, valY);
        }
        
        tft.setTextFont(2);
        tft.drawString("%", card2_X + cardW/2 + 45, valY - 5);
        
        drawProgressBar(tft, card2_X + 10, cardY + cardH - 8, cardW - 20, 4, hum, 0, 100, humColor);
    } else {
        tft.setTextColor(TFT_RED, 0x1082);
        tft.setTextFont(4);
        tft.drawString("--", card2_X + cardW/2, cardY + cardH/2);
    }
    
    // =========================================================
    // STOPKA SYSTEMOWA (TYLKO TRYB ONLINE)
    // =========================================================
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextSize(1);
    
    tft.fillRect(0, UPDATES_CLEAR_Y, 320, 240 - UPDATES_CLEAR_Y, COLOR_BACKGROUND);
    
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("STATUS SYSTEMU:", 160, UPDATES_TITLE_Y);
    
    // 1. STATUS SENSORA (Używamy stałej UPDATES_DHT22_Y - pozycja 160)
    String sensorStatusLine = sensorName + ": " + sensorStatusMsg;
    uint16_t statusColor = isValid ? TFT_GREEN : TFT_RED;
    tft.setTextColor(statusColor);
    tft.drawString(sensorStatusLine, 160, UPDATES_DHT22_Y); 
    
    // 2. INTERWAŁ ODCZYTU (Używamy stałej UPDATES_SENSOR_Y - pozycja 175)
    tft.setTextColor(TFT_DARKGREY);
    String sensorInterval = "Odczyt sensora: co " + String(readIntervalSec) + "s";
    tft.drawString(sensorInterval, 160, UPDATES_SENSOR_Y);
    
    // POGODA
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
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(wPrefix, wX, UPDATES_WEATHER_Y);
    wX += tft.textWidth(wPrefix);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(wTime, wX, UPDATES_WEATHER_Y);
    wX += tft.textWidth(wTime);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(wSuffix, wX, UPDATES_WEATHER_Y);
    
    // PROGNOZA TYGODNIOWA
    unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
    String fPrefix = "Pogoda tyg.: ";
    String fTime;
    if (weeklyAge < 60) fTime = String(weeklyAge) + "s temu";
    else if (weeklyAge < 3600) fTime = String(weeklyAge / 60) + "min temu";
    else fTime = String(weeklyAge / 3600) + "h temu";
    String fSuffix = " (co 4h)";
    
    int fTotalWidth = tft.textWidth(fPrefix + fTime + fSuffix);
    int fX = 160 - (fTotalWidth / 2);
    
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(fPrefix, fX, UPDATES_WEEKLY_Y);
    fX += tft.textWidth(fPrefix);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(fTime, fX, UPDATES_WEEKLY_Y);
    fX += tft.textWidth(fTime);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(fSuffix, fX, UPDATES_WEEKLY_Y);
    
    // WIFI
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
    
    // WERSJA
    tft.setTextDatum(BR_DATUM);
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    String versionText = "v" + String(FIRMWARE_VERSION);
    tft.drawString(versionText, tft.width() - 5, tft.height() - 5);
  }
  
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
}