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

#ifndef UPDATES_TITLE_Y
  #define UPDATES_CLEAR_Y   130
  #define UPDATES_TITLE_Y   140
  #define UPDATES_DHT22_Y   160  
  #define UPDATES_SENSOR_Y  175  
  #define UPDATES_WEATHER_Y 190
  #define UPDATES_WEEKLY_Y  205
  #define UPDATES_WIFI_Y    220
  #define UPDATES_BATTERY_Y 235
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

static bool readBatteryVoltage(float& outVoltage) {
    const uint8_t batteryPin = getBatteryAdcPin();
    if (batteryPin == 255) {
        return false;
    }

    //"Budzik" dla ADC po Deep Sleep ---
    analogRead(batteryPin); 
    delay(5);

    const uint32_t adcMilliVolts = analogReadMilliVolts(batteryPin);
    if (adcMilliVolts == 0) {
        return false;
    }

    outVoltage = (adcMilliVolts / 1000.0f) * getBatteryDividerRatio();
    return true;
}

// Eliminuje miganie poprzez unikanie czyszczenia całego paska.
// Czyści tylko lewy i prawy margines, a tekst nadpisuje "w locie".
void drawCenteredCompactLine(TFT_eSPI& tft, int y, String label, String timeText, String suffix) {
    tft.setTextSize(1);
    
    // 1. Obliczamy szerokości
    int wLabel = tft.textWidth(label);
    int wTime = tft.textWidth(timeText);
    int wSuffix = tft.textWidth(suffix);
    
    int totalWidth = wLabel + wTime + wSuffix;
    int startX = (320 - totalWidth) / 2;
    
    // 2. CZYŚCIMY TYLKO LEWY MARGINES (od 0 do początku tekstu)
    // To usuwa śmieci po lewej stronie, jeśli tekst się przesunął
    if (startX > 0) {
        tft.fillRect(0, y, startX, 15, COLOR_BACKGROUND);
    }
    
    // 3. RYSUJEMY TEKST (Z tłem pod literami - to zapobiega miganiu tekstu)
    int currentX = startX;
    tft.setTextDatum(TL_DATUM); 
    
    // Etykieta
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    tft.drawString(label, currentX, y);
    currentX += wLabel;
    
    // Czas
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.drawString(timeText, currentX, y);
    currentX += wTime;
    
    // Sufiks
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    tft.drawString(suffix, currentX, y);
    currentX += wSuffix;
    
    // 4. CZYŚCIMY TYLKO PRAWY MARGINES (od końca tekstu do 320)
    // To usuwa śmieci po prawej, jeśli tekst się skrócił
    if (currentX < 320) {
        tft.fillRect(currentX, y, 320 - currentX, 15, COLOR_BACKGROUND);
    }
}

void displayLocalSensors(TFT_eSPI& tft, bool onlyUpdate) {
  
  tft.setTextSize(1);

  // 1. TŁO (Tylko raz)
  if (!onlyUpdate) {
      Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS (NO BLINK)");
      tft.fillScreen(COLOR_BACKGROUND);
  }

  // 3. RESET USTAWIEŃ
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND); 

  // DANE SENSORÓW
  float temp = 0.0;
  float hum = 0.0;
  bool isValid = false;
  
  String sensorName = "";
  String sensorStatusMsg = "";
  int readIntervalSec = 0;
  int tempDecimals = 1; 
  bool humIsInt = true;

  #ifdef USE_SHT31
    temp = localTemperature;
    hum = localHumidity;
    isValid = (hum != 0.0 && !isnan(temp)); 
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
    sensorName = "DHT22";
    sensorStatusMsg = dhtData.status;
    readIntervalSec = (DHT22_READ_INTERVAL / 1000);
    tempDecimals = 1;
    humIsInt = true;
  #endif

  // ##################################################################
  // TRYB OFFLINE (DUŻE KARTY)
  // ##################################################################
  if (isOfflineMode) {
    uint8_t cardStartY = 70;   
    uint8_t cardH = 95;        
    uint8_t cardW = 145;       
    uint8_t card1_X = 10;
    uint8_t card2_X = 165;

    // --- STATYCZNE (Rysowane tylko przy zmianie ekranu) ---
    if (!onlyUpdate) {
        uint8_t headerY = 55;
        tft.drawFastHLine(0, headerY, 320, TFT_DARKGREY); 
        tft.setTextColor(TFT_SILVER, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WARUNKI W POMIESZCZENIU", 160, headerY - 10);

        // Kafelki
        tft.fillRoundRect(card1_X, cardStartY, cardW, cardH, 6, CARD_BG_COLOR);
        tft.fillRoundRect(card2_X, cardStartY, cardW, cardH, 6, CARD_BG_COLOR);
        tft.drawRoundRect(card1_X, cardStartY, cardW, cardH, 6, TFT_DARKGREY);
        tft.drawRoundRect(card2_X, cardStartY, cardW, cardH, 6, TFT_DARKGREY);

        tft.setTextColor(TFT_ORANGE, CARD_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("TEMP", card1_X + cardW/2, cardStartY + 10);
        tft.setTextColor(TFT_CYAN, CARD_BG_COLOR);
        tft.drawString("WILGOTNOSC", card2_X + cardW/2, cardStartY + 10);

        tft.drawRoundRect(card1_X + 8, cardStartY + cardH - 25, cardW - 16, 6, 3, TFT_DARKGREY);
        tft.drawRoundRect(card2_X + 8, cardStartY + cardH - 25, cardW - 16, 6, 3, TFT_DARKGREY);

        // ==========================================
        // PROFESJONALNA STOPKA - ELEMENTY STATYCZNE
        // ==========================================
        int footerLineY = 175; 
        tft.drawFastHLine(10, footerLineY, 300, 0x39E7); // Stalowy szary
        
        // Zmiana koloru na TFT_ORANGE 
        tft.setTextSize(1);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_ORANGE, COLOR_BACKGROUND);
        tft.drawString("[ SYSTEM OFFLINE ]", 160, footerLineY + 18);
    }

    // --- DYNAMICZNE (Rysowane i aktualizowane w locie) ---
    tft.setTextSize(1);
    int valY = cardStartY + 48;
    
    // Temperatura
    if (isValid) {
        uint16_t tempColor = TFT_GREEN;
        if (temp < 18) tempColor = TFT_CYAN;
        if (temp > 24) tempColor = TFT_ORANGE;
        if (temp > 28) tempColor = TFT_RED;
        tft.setTextColor(tempColor, CARD_BG_COLOR); 
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(temp, tempDecimals), card1_X + cardW/2 - 5, valY);
        
        int unitX = card1_X + cardW/2 + 40;
        int unitY = valY;

        tft.setTextFont(2);
        tft.fillCircle(unitX - 9, unitY - 6, 2, tempColor);
        tft.drawString("C", unitX, unitY);    

        drawProgressBar(tft, card1_X + 8, cardStartY + cardH - 25, cardW - 16, 6, temp, 0, 40, tempColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--.-", card1_X + cardW/2, cardStartY + 48);
    }

    // Wilgotność
    if (isValid) {
        uint16_t humColor = TFT_GREEN;
        if (hum < 30) humColor = TFT_YELLOW;
        if (hum > 60) humColor = TFT_BLUE;
        tft.setTextColor(humColor, CARD_BG_COLOR); 
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        if (humIsInt) tft.drawString(String((int)hum), card2_X + cardW/2 - 5, valY);
        else tft.drawString(String(hum, 2), card2_X + cardW/2 - 5, valY);

        tft.setTextDatum(BL_DATUM);
        tft.setTextFont(2);
        tft.drawString(" %", card2_X + cardW/2 + 27, valY + 8);
        drawProgressBar(tft, card2_X + 8, cardStartY + cardH - 25, cardW - 16, 6, hum, 0, 100, humColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("--", card2_X + cardW/2, cardStartY + 48);
    }

    // ==========================================
    // PROFESJONALNA STOPKA - ELEMENTY DYNAMICZNE
    // ==========================================
    int footerTextY = 215; 
    tft.setTextSize(1);
    tft.setTextFont(2);

    // 1. LEWA STRONA: Mikro-ikona baterii + Zasilanie (Bez Mrugania)
    int batX = 10;
    
    float batteryVoltage = 0.0f;
    if (readBatteryVoltage(batteryVoltage)) {
        
        // Czyścimy TYLKO malutki obszar samej grafiki baterii
        tft.fillRect(batX, footerTextY, 24, 15, COLOR_BACKGROUND);
        
        // Rysowanie obwódki ikony baterii
        tft.drawRect(batX, footerTextY + 2, 20, 10, TFT_DARKGREY);
        tft.fillRect(batX + 20, footerTextY + 5, 2, 4, TFT_DARKGREY);
        
        tft.setTextDatum(TL_DATUM);
        
        // LOGIKA 1: BRAK BATERII (Fantomy / Pływający pin poniżej 1.0V)
        if (batteryVoltage < 1.0f) {
            // Nie rysujemy paska w środku baterii (zostaje pusta)
            tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
            tft.drawString("PWR: ", batX + 28, footerTextY);
            tft.setTextColor(TFT_CYAN, COLOR_BACKGROUND); // Cyjan dla zasilania zewnętrznego
            tft.drawString(" USB      ", batX + 58, footerTextY);
        }
        // LOGIKA 2: ŁADOWANIE LUB USB (Powyżej 4.15V)
        else if (batteryVoltage > 4.15f) {
            tft.fillRect(batX + 2, footerTextY + 4, 16, 6, TFT_GREEN); // Pełna bateria
            tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
            tft.drawString("PWR: ", batX + 28, footerTextY);
            tft.setTextColor(TFT_GREEN, COLOR_BACKGROUND);
            tft.drawString("USB / CHRG  ", batX + 58, footerTextY); 
        } 
        // LOGIKA 3: NORMALNA PRACA NA BATERII
        else {
            uint16_t batColor = TFT_GREEN;
            if (batteryVoltage < 3.45f) batColor = TFT_RED;
            else if (batteryVoltage < 3.65f) batColor = TFT_ORANGE;
            
            float pct = (batteryVoltage - 3.2f) / 1.0f; 
            if (pct > 1.0f) pct = 1.0f;
            if (pct < 0.0f) pct = 0.0f;
            int fillW = (int)(16 * pct);
            if (fillW > 0) tft.fillRect(batX + 2, footerTextY + 4, fillW, 6, batColor);
            
            tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
            tft.drawString("BAT: ", batX + 28, footerTextY);
            tft.setTextColor(batColor, COLOR_BACKGROUND);
            tft.drawString(String(batteryVoltage, 2) + "V   ", batX + 58, footerTextY); 
        }
    } else {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.drawString("BAT: ERR          ", batX, footerTextY);
    
    }

    // 2. PRAWA STRONA: Dioda statusu + Sensor (Bez Mrugania)
    int sensX = 310;
    
    uint16_t sensorColor = isValid ? TFT_GREEN : TFT_RED;
    String statusStr = isValid ? "OK" : "BLAD";
    
    tft.setTextDatum(TR_DATUM);
    
    // Rysowanie z tłem eliminuje potrzebę fillRect
    tft.setTextColor(sensorColor, COLOR_BACKGROUND);
    tft.drawString(statusStr + "  ", sensX, footerTextY); // Spacje zapobiegają ucinaniu
    
    int statusWidth = tft.textWidth(statusStr + "  ");
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    tft.drawString(sensorName + " ", sensX - statusWidth, footerTextY);
    
    // Diodę rysujemy na czystym fragmencie (żeby nie zostawiała ducha po zmianie koloru)
    int nameWidth = tft.textWidth(sensorName + " ");
    int dotX = sensX - statusWidth - nameWidth - 6;
    tft.fillRect(dotX - 4, footerTextY + 4, 8, 8, COLOR_BACKGROUND);
    tft.fillCircle(dotX, footerTextY + 7, 3, sensorColor);
    // ==========================================
  
  
  } 
  // ##################################################################
  // TRYB ONLINE (KOMPAKT + STOPKA ZERO MIGANIA)
  // ##################################################################
  else {
    
    uint8_t cardY = 55;
    int cardH = 70;
    uint8_t cardW = 135;
    uint8_t card1_X = 20;
    uint8_t card2_X = 165;
    
    // --- STATYCZNE (Tylko raz) ---
    if (!onlyUpdate) {
        uint8_t headerY = 45;
        tft.drawFastHLine(20, headerY, 280, TFT_DARKGREY);
        tft.setTextColor(TFT_SILVER, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WARUNKI W POMIESZCZENIU", tft.width() / 2, 35);
        
        tft.fillRoundRect(card1_X, cardY, cardW, cardH, 8, CARD_BG_COLOR);
        tft.fillRoundRect(card2_X, cardY, cardW, cardH, 8, CARD_BG_COLOR);
        tft.drawRoundRect(card1_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
        tft.drawRoundRect(card2_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
        
        tft.setTextColor(TFT_ORANGE, CARD_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("TEMP", card1_X + cardW/2, cardY + 15);
        tft.setTextColor(TFT_CYAN, CARD_BG_COLOR);
        tft.drawString("WILGOTNOSC", card2_X + cardW/2, cardY + 15);
        
        int valY = cardY + 40;
        tft.setTextColor(TFT_GREEN, CARD_BG_COLOR);
        tft.setTextFont(2);
        
        tft.drawRoundRect(card1_X + 10, cardY + cardH - 8, cardW - 20, 4, 2, TFT_DARKGREY);
        tft.drawRoundRect(card2_X + 10, cardY + cardH - 8, cardW - 20, 4, 2, TFT_DARKGREY);
        
        // STOPKA TYTUŁ
        tft.fillRect(0, UPDATES_CLEAR_Y, 320, 240 - UPDATES_CLEAR_Y, COLOR_BACKGROUND);
        tft.setTextFont(1);
        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_CYAN, COLOR_BACKGROUND);
        tft.drawString("STATUS SYSTEMU:", 160, UPDATES_TITLE_Y);
    }
    
    // --- DYNAMICZNE (LICZBY) ---
    tft.setTextSize(1);
    int valY = cardY + 40;
    
    if (isValid) {
        uint16_t tempColor = TFT_GREEN;
        if (temp < 18) tempColor = TFT_CYAN;
        if (temp > 24) tempColor = TFT_ORANGE;
        if (temp > 28) tempColor = TFT_RED;
        tft.setTextColor(tempColor, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(temp, tempDecimals), card1_X + cardW/2, valY);

        int unitX = card1_X + cardW/2 + 47;
        int unitY = valY;

        tft.setTextFont(2);
        tft.fillCircle(unitX - 9, unitY - 6, 2, tempColor);  // °
        tft.drawString("C", unitX, unitY);    

        drawProgressBar(tft, card1_X + 10, cardY + cardH - 8, cardW - 20, 4, temp, 0, 40, tempColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.drawString("--.-", card1_X + cardW/2, cardY + cardH/2);
    }
    
    if (isValid) {
        uint16_t humColor = TFT_GREEN;
        if (hum < 30) humColor = TFT_YELLOW;
        if (hum > 60) humColor = TFT_BLUE;
        tft.setTextColor(humColor, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        if (humIsInt) tft.drawString(String((int)hum), card2_X + cardW/2, valY);
        else tft.drawString(String(hum, 2), card2_X + cardW/2, valY);

        tft.setTextDatum(BL_DATUM);
        tft.setTextFont(2);
        tft.drawString(" %", card2_X + cardW/2 + 33, valY + 8);

        drawProgressBar(tft, card2_X + 10, cardY + cardH - 8, cardW - 20, 4, hum, 0, 100, humColor);
    } else {
        tft.setTextColor(TFT_RED, CARD_BG_COLOR);
        tft.setTextFont(4);
        tft.drawString("--", card2_X + cardW/2, cardY + cardH/2);
    }

    // --- DYNAMICZNE (STOPKA) ---
    tft.setTextFont(1);
    tft.setTextSize(1);
    
    // Status Sensora i Interval
    tft.setTextDatum(TC_DATUM);
    tft.setTextPadding(200); 

    String statusLine = (sensorName == "" ? "SHT31" : sensorName) + ": " + sensorStatusMsg;
    uint16_t statusColor = isValid ? TFT_GREEN : TFT_RED;
    tft.setTextColor(statusColor, COLOR_BACKGROUND);
    tft.drawString(statusLine, 160, UPDATES_DHT22_Y); 
    
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    String intervalLine = "Odczyt sensora: co " + String(readIntervalSec) + "s";
    tft.drawString(intervalLine, 160, UPDATES_SENSOR_Y);
    tft.setTextPadding(0);

    // --- POGODA (IDEALNIE WYSRODKOWANA, BEZ MIGANIA) ---
    unsigned long weatherAge = (millis() - lastWeatherCheckGlobal) / 1000;
    String wTime;
    if (weatherAge < 60) wTime = String(weatherAge) + "s temu";
    else if (weatherAge < 3600) wTime = String(weatherAge / 60) + "min temu";
    else wTime = String(weatherAge / 3600) + "h temu";
    
    drawCenteredCompactLine(tft, UPDATES_WEATHER_Y, "Pogoda: ", wTime, " (co 10min)");

    // --- WEEKLY (IDEALNIE WYSRODKOWANA, BEZ MIGANIA) ---
    unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
    String fTime;
    if (weeklyAge < 60) fTime = String(weeklyAge) + "s temu";
    else if (weeklyAge < 3600) fTime = String(weeklyAge / 60) + "min temu";
    else fTime = String(weeklyAge / 3600) + "h temu";
    
    drawCenteredCompactLine(tft, UPDATES_WEEKLY_Y, "Pogoda tyg.: ", fTime, " (co 4h)");

    // WiFi 
    tft.setTextDatum(TC_DATUM);
    tft.setTextPadding(320); 
    String wifiTxt = (WiFi.status() == WL_CONNECTED) ? "WiFi: " + String(WiFi.SSID()) : "WiFi: Rozlaczony";
    uint16_t wifiColor = (WiFi.status() == WL_CONNECTED) ? TFT_DARKGREY : TFT_RED;
    tft.setTextColor(wifiColor, COLOR_BACKGROUND);
    tft.drawString(wifiTxt, 160, UPDATES_WIFI_Y);

   // ==========================================
    // INTELIGENTNY STATUS ZASILANIA (ONLINE)
    // ==========================================
    float batteryVoltage = 0.0f;
    if (readBatteryVoltage(batteryVoltage)) {
        String labelTxt;
        String valTxt;
        uint16_t valColor;

        // LOGIKA 1: BRAK BATERII (Fantomy poniżej 1.0V)
        if (batteryVoltage < 1.0f) {
            labelTxt = "Zasilanie: ";
            valTxt = "USB";
            valColor = TFT_CYAN; 
        } 
        // LOGIKA 2: ŁADOWANIE LUB USB (Powyżej 4.15V)
        else if (batteryVoltage > 4.17f) {
            labelTxt = "Zasilanie: ";
            valTxt = "USB";
            valColor = TFT_GREEN;
        } 
        // LOGIKA 3: NORMALNA PRACA NA BATERII
        else {
            labelTxt = "Bateria: ";
            valTxt = String(batteryVoltage, 2) + "V";
            valColor = TFT_DARKGREY;
            if (batteryVoltage < 3.45f) valColor = TFT_RED;
            else if (batteryVoltage < 3.65f) valColor = TFT_ORANGE;
            else valColor = TFT_GREEN;
        }

        //printf("[DEBUG] Stan baterii: %f\n", batteryVoltage);
        
        tft.setTextPadding(0); // Wyłączamy padding automatyczny
        
        // Obliczamy szerokości, żeby idealnie wyśrodkować całość
        int wLabel = tft.textWidth(labelTxt);
        int wVal = tft.textWidth(valTxt);
        int totalW = wLabel + wVal;
        int startX = (320 - totalW) / 2;
        
        // 1. Czyścimy TYLKO lewy margines (żeby zapobiec mruganiu)
        if (startX > 0) tft.fillRect(0, UPDATES_BATTERY_Y, startX, 15, COLOR_BACKGROUND);
        
        tft.setTextDatum(TL_DATUM); // Rysujemy od lewej do prawej
        
        // 2. Rysujemy szarą etykietę
        tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
        tft.drawString(labelTxt, startX, UPDATES_BATTERY_Y);
        
        // 3. Rysujemy kolorową wartość zaraz za etykietą
        tft.setTextColor(valColor, COLOR_BACKGROUND);
        tft.drawString(valTxt, startX + wLabel, UPDATES_BATTERY_Y);
        
        // 4. Czyścimy TYLKO prawy margines
        int endX = startX + totalW;
        if (endX < 320) tft.fillRect(endX, UPDATES_BATTERY_Y, 320 - endX, 15, COLOR_BACKGROUND);
        
    } else {
        tft.setTextPadding(320);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.drawString("Bateria: Blad odczytu", 160, UPDATES_BATTERY_Y);
    }
    // ==========================================

    tft.setTextPadding(0); // Zdejmuje padding dla numeru wersji poniżej
    
    tft.setTextDatum(BR_DATUM);
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    tft.drawString("v" + String(FIRMWARE_VERSION), 315, 235);
  }
  
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
}
