#include "display/current_weather_display.h"
#include "managers/WeatherCache.h"
#include "wifi/wifi_touch_interface.h"
#include "display/weather_icons.h"
#include "config/display_config.h"
#include "weather/forecast_data.h"

// Singleton instance WeatherCache  
static WeatherCache weatherCache;

WeatherCache& getWeatherCache() {
  return weatherCache;
}

bool hasWeatherChanged() {
  return getWeatherCache().hasChanged(weather);
}

void updateWeatherCache() {
  getWeatherCache().updateCache(weather);
}

// ================================================================
// FUNKCJE POMOCNICZE
// ================================================================

String shortenDescription(String description) {
    String polishDescription = description;
    if (description.indexOf("thunderstorm with heavy rain") >= 0) polishDescription = "Burza z ulewa";
    else if (description.indexOf("thunderstorm with rain") >= 0) polishDescription = "Burza z deszczem";
    else if (description.indexOf("thunderstorm") >= 0) polishDescription = "Burza";
    else if (description.indexOf("drizzle") >= 0) polishDescription = "Mzawka";
    else if (description.indexOf("heavy intensity rain") >= 0) polishDescription = "Ulewa";
    else if (description.indexOf("moderate rain") >= 0) polishDescription = "Umiarkowany deszcz";
    else if (description.indexOf("light rain") >= 0) polishDescription = "Slaby deszcz";
    else if (description.indexOf("rain") >= 0) polishDescription = "Deszcz";
    else if (description.indexOf("snow") >= 0) polishDescription = "Snieg";
    else if (description.indexOf("sleet") >= 0) polishDescription = "Deszcz ze sniegiem";
    else if (description.indexOf("mist") >= 0) polishDescription = "Mgla";
    else if (description.indexOf("fog") >= 0) polishDescription = "Mgla";
    else if (description.indexOf("clear sky") >= 0) polishDescription = "Bezchmurnie";
    else if (description.indexOf("overcast clouds") >= 0) polishDescription = "Pochmurno";
    else if (description.indexOf("broken clouds") >= 0) polishDescription = "Duze zachmurzenie";
    else if (description.indexOf("scattered clouds") >= 0) polishDescription = "Srednie zachmurzenie";
    else if (description.indexOf("few clouds") >= 0) polishDescription = "Male zachmurzenie";
    return polishDescription;
}

uint16_t getWindColor(float windKmh) {
    if (windKmh >= 30.0) return TFT_MAROON;
    else if (windKmh >= 25.0) return TFT_RED;
    else if (windKmh >= 20.0) return TFT_YELLOW;
    else return TFT_WHITE;
}

uint16_t getPressureColor(float pressure) {
    if (pressure < 1000.0) return TFT_ORANGE;
    else if (pressure > 1020.0) return TFT_MAGENTA;
    else return TFT_WHITE;
}

uint16_t getHumidityColor(float humidity) {
    if (humidity < 30.0) return TFT_RED;
    else if (humidity > 90.0) return 0x7800;
    else if (humidity > 85.0) return TFT_PURPLE;
    else return TFT_WHITE;
}

// ================================================================
// FUNKCJA RYSOWANIA STRZAŁKI (Wersja MINI - 5x5 px)
// ================================================================
void drawTrendArrow(TFT_eSPI& tft, int x, int y, int direction, uint16_t color) {
    // ZMNIEJSZONE WYMIARY
    int w = 5; 
    int h = 5; 
    
    // Tło czyszczące ("Gumka") - przywrócone i dopasowane
    // Musi być włączone, żeby strzałki nie nakładały się na siebie przy zmianie pogody
    // Zakres Y: od -2 do +14 (pokrywa Twoje offsety 0 i 5)
    tft.fillRect(x - 4, y - 2, 9, 10, 0x1082); 
    
    // Kod 99 oznacza "Brak ikony" (dla dolnej linii w trybie stabilnym)
    if (direction == 99) return;

    if (direction >= 1) { 
        // GÓRA 
        // Twój offset: 5
        int off = 5; 
        tft.fillTriangle(x, y - h + off, x - w/2, y + off, x + w/2, y + off, color);
    } 
    else if (direction <= -1) { 
        // DÓŁ 
        // Twój offset: 0
        int off = 0; 
        tft.fillTriangle(x, y + h + off, x - w/2, y + off, x + w/2, y + off, color);
    }
    else { 
        // KRESKA (Stabilnie)
        // Twoja pozycja: y + 6
        tft.fillRect(x - 2, y + 6, 5, 2, color); 
    }
}


// ================================================================
// LOGIKA TRENDU CIŚNIENIA - FINALNA (REALNE DANE + TWOJE STRINGI)
// ================================================================

void getPressureTrendInfo(float currentPressure, String &trendText, String &forecastText, uint16_t &trendColor, int &dir1, int &dir2) {
    // --- KONFIGURACJA ---
    const int HISTORY_SIZE = 13;       // 12 slotów po 15 min = 3h (+1 na bieżący)
    const unsigned long INTERVAL = 15 * 60 * 1000UL; // Co 15 minut zapisujemy punkt
    
    // Zmienne statyczne
    static float history[HISTORY_SIZE] = {0}; 
    static bool isInitialized = false;
    static unsigned long lastUpdate = 0;
    static int count = 0; 

    // 1. INICJALIZACJA
    if (!isInitialized || history[0] == 0) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            history[i] = currentPressure;
        }
        isInitialized = true;
        lastUpdate = millis();
        count = 1;
    }

    // 2. AKTUALIZACJA HISTORII
    if (millis() - lastUpdate >= INTERVAL) {
        lastUpdate = millis();
        for (int i = HISTORY_SIZE - 1; i > 0; i--) {
            history[i] = history[i - 1];
        }
        history[0] = currentPressure; 
        if (count < HISTORY_SIZE) count++; 
    }
    
    // 3. OBLICZANIE RÓŻNICY
    float pressure3hAgo = history[count - 1]; 
    float diff = currentPressure - pressure3hAgo;

    // 4. USTALANIE STATUSU (PROGI)
    float thresholdStable = 0.5; // +/- 0.5 hPa 
    float thresholdStorm = 4.0;  // +/- 4.0 hPa

    // Logika mapowania trendu (Twoje stringi z testu)
    
    if (diff > thresholdStorm) {
        // Szybko rośnie
        trendText = "Szybko rosnie";
        forecastText = "Wiatr Bezchm.";
        trendColor = TFT_MAGENTA;
        dir1 = -1; // Strzałka dół
        dir2 = -1; // Strzałka dół
        
    } else if (diff > thresholdStable) {
        // Rośnie
        trendText = "Rosnie";
        forecastText = "Wiatr Chmury"; // Zmienione na "Chmury" (zgodnie z testem)
        trendColor = TFT_GREEN;
        dir1 = -1; // Strzałka dół
        dir2 = -1; // Strzałka dół
        
    } else if (diff < -thresholdStorm) {
        // Gwałtownie spada
        trendText = "Spada gwaltownie"; // Zmienione na "Spada gwaltownie"
        forecastText = "Wiatr BURZA!";
        trendColor = TFT_RED;
        dir1 = 2; // Strzałka góra
        dir2 = 2; // Strzałka góra
        
    } else if (diff < -thresholdStable) {
        // Spada
        trendText = "Spada";
        forecastText = "Wiatr Pochm.";
        trendColor = TFT_ORANGE;
        dir1 = 1; // Strzałka góra
        dir2 = 1; // Strzałka góra
        
    } else {
        // Stabilnie
        trendText = "Stabilne";
        forecastText = "Pogoda Bez zm."; // Zmienione na "Bez zm."
        trendColor = TFT_CYAN;
        dir1 = 0;  // Kreska
        dir2 = 99; // Brak ikony
    }
}


// ================================================================
// GŁÓWNA FUNKCJA WYŚWIETLANIA POGODY
// ================================================================

void displayCurrentWeather(TFT_eSPI& tft) {
    if (isWiFiConfigActive()) {
        return; 
    }
    
    if (!weather.isValid) {
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(1);
        tft.setTextSize(2);
        tft.drawString("BRAK DANYCH..", 160, 100);
        return;
    }

    tft.setTextFont(1);

    uint8_t height = 175;
    uint16_t CARD_BG = 0x1082;      
    uint16_t TEXT_BG = CARD_BG;  
    uint16_t BORDER_COLOR = TFT_DARKGREY;
    uint16_t LABEL_COLOR = TFT_SILVER;

    uint8_t startY = 5;
    uint8_t startX = 60;
    
    // =========================================================
    // LEWA KARTA Z TEMPERATURĄ
    // =========================================================
    tft.fillRoundRect(5, startY, 150, height, 8, TFT_BLACK);  
    tft.drawRoundRect(5, startY, 150, height, 8, BORDER_COLOR); 

    String polishDesc = shortenDescription(weather.description);
    polishDesc.toUpperCase();

    tft.setTextDatum(MC_DATUM);
    drawWeatherIcon(tft, startX - 5, WEATHER_CARD_TEMP_Y_OFFSET - 10, weather.description, weather.icon);

    uint16_t tempColor = TFT_WHITE;
    if (weather.temperature < 0 || weather.feelsLike < 0) tempColor = TFT_CYAN;
    else if (weather.temperature > 30) tempColor = TFT_RED;
    else if (weather.temperature > 25) tempColor = TFT_ORANGE;

    tft.setTextColor(tempColor, TFT_BLACK); 
    tft.setTextSize(5);
    tft.setTextDatum(MC_DATUM);
    String tempStr = String((int)round(weather.temperature));
    tft.drawString(tempStr, 80, startY + 60 + WEATHER_CARD_TEMP_Y_OFFSET);

    uint16_t descColor = TFT_CYAN;
    if (polishDesc.indexOf("BURZA") >= 0) descColor = TFT_RED;
    else if (polishDesc == "BEZCHMURNIE") descColor = TFT_YELLOW;
    else if (polishDesc == "MGLA") descColor = TFT_WHITE;

    tft.setTextColor(descColor, TFT_BLACK);
    tft.setTextSize(2);
    if (tft.textWidth(polishDesc) > 140) tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(polishDesc, 80, startY + 95 + WEATHER_CARD_TEMP_Y_OFFSET);

    tft.setTextColor(LABEL_COLOR, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("ODCZUWALNA:", 80, startY + 116 + WEATHER_CARD_TEMP_Y_OFFSET);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(String((int)round(weather.feelsLike)), 80, startY + 135 + WEATHER_CARD_TEMP_Y_OFFSET);

    // =========================================================
    // PRAWA KOLUMNA
    // =========================================================
    uint8_t rowH = 39; 
    uint8_t gap = 6; 
    uint8_t rightX = 165; 
    uint8_t rightW = 150;

    // --- 1. WILGOTNOŚĆ ---
    uint8_t y1 = startY;
    tft.fillRoundRect(rightX, y1, rightW, rowH, 6, CARD_BG);
    tft.drawRoundRect(rightX, y1, rightW, rowH, 6, BORDER_COLOR);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("WILGOTNOSC", rightX + 5, y1 + 5);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(getHumidityColor(weather.humidity), TEXT_BG);
    tft.setTextSize(3);
    tft.drawString(String((int)weather.humidity) + "%", rightX + rightW - 5, y1 + 12);

    // --- 2. WIATR ---
    uint8_t y2 = y1 + rowH + gap;
    tft.fillRoundRect(rightX, y2, rightW, rowH, 6, CARD_BG);
    tft.drawRoundRect(rightX, y2, rightW, rowH, 6, BORDER_COLOR);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("WIATR", rightX + 5, y2 + 5);

    float windKmh = weather.windSpeed * 3.6;
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(getWindColor(windKmh), TEXT_BG);
    tft.setTextSize(3);
    tft.drawString(String((int)round(windKmh)), rightX + rightW - 35, y2 + 12);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    tft.drawString("km/h", rightX + rightW - 5, y2 + 27);

    // --- 3. CIŚNIENIE ---
    uint8_t y3 = y2 + rowH + gap;
    tft.fillRoundRect(rightX, y3, rightW, rowH, 6, CARD_BG);
    tft.drawRoundRect(rightX, y3, rightW, rowH, 6, BORDER_COLOR);
    
    String trendTxt, forecastTxt;
    uint16_t trendCol;
    int dir1, dir2; 
    
    // Pobieramy dane z logiki trendu
    getPressureTrendInfo(weather.pressure, trendTxt, forecastTxt, trendCol, dir1, dir2);

    // Etykieta CIS.
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("CIS.", rightX + 5, y3 + 5);

    // Trend
    tft.setTextColor(trendCol, TEXT_BG);
    tft.drawString(trendTxt, rightX + 35, y3 + 5);

    // --- DOLNA SEKCJA ---
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    tft.setTextSize(1);
    
    int textXOffset = 22; 
    int arrowX = rightX + 12;
    
    // TWOJE NOWE POZYCJE LINII
    int row1_Y = y3 + 17; 
    int row2_Y = y3 + 30; 
    
    // Rysujemy strzałki
    drawTrendArrow(tft, arrowX, row1_Y, dir1, trendCol);
    drawTrendArrow(tft, arrowX, row2_Y, dir2, trendCol);
    
    // Rysujemy teksty
    int spaceIndex = forecastTxt.indexOf(' ');
    
    if (spaceIndex > 0) {
        String line1 = forecastTxt.substring(0, spaceIndex); 
        String line2 = forecastTxt.substring(spaceIndex + 1); 
        
        tft.drawString(line1, rightX + textXOffset, row1_Y);
        tft.drawString(line2, rightX + textXOffset, row2_Y);
    } else {
        tft.drawString(forecastTxt, rightX + textXOffset, row1_Y + 5);
    }
    
    // Wartość ciśnienia
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(getPressureColor(weather.pressure), TEXT_BG);
    tft.setTextSize(2);
    tft.drawString(String((int)weather.pressure), rightX + rightW - 30, y3 + 20); 

    // Jednostka
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    tft.drawString("hPa", rightX + rightW - 5, y3 + 27);

    // --- 4. OPADY ---
    uint8_t y4 = y3 + rowH + gap;
    tft.fillRoundRect(rightX, y4, rightW, rowH , 6, CARD_BG);
    tft.drawRoundRect(rightX, y4, rightW, rowH , 6, BORDER_COLOR);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("SZANSA NA OPADY", rightX + 5, y4 + 5);

    String rainVal = "--";
    uint16_t rainColor = LABEL_COLOR;
    if (forecast.isValid && forecast.count > 0) {
        uint8_t chance = forecast.items[0].precipitationChance;
        rainVal = String(chance) + "%";
        rainColor = (chance > 0) ? TFT_SKYBLUE : LABEL_COLOR;
    }

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(rainColor, TEXT_BG);
    tft.setTextSize(2);
    tft.drawString(rainVal, rightX + rightW - 5, y4 + 12);

    updateWeatherCache();
}