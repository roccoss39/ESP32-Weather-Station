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
// FUNKCJA RYSOWANIA STRZAŁKI (Wyrównana idealnie do linii tekstu)
// ================================================================
void drawTrendArrow(TFT_eSPI& tft, int x, int y, int direction, uint16_t color) {
    int w = 7;
    int h = 9; // Wysokość strzałki
    
    // Czyścimy tło
    tft.fillRect(x - 5, y - 8, 11, 20, 0x1082); 

    // BAZA: Funkcja jest wywoływana na Y = 24.
    // Tekst "Wiatr" jest na Y = 18.

    if (direction == 1) { // GÓRA (Wiatr rośnie)
        // off = 3 -> Wierzchołek: 24 - 9 + 3 = 18 (IDEALNIE RÓWNO Z TEKSTEM)
        int off = 3; 
        tft.fillTriangle(x, y - h + off, x - w/2, y + off, x + w/2, y + off, color);
    } 
    else if (direction == 2) { // PODWÓJNA GÓRA
        int off = 3; 
        tft.fillTriangle(x, y - h + off, x - w/2, y + off, x + w/2, y + off, color);
        tft.fillTriangle(x, y + off, x - w/2, y + h + off, x + w/2, y + h + off, color);
    }
    else if (direction == -1) { // DÓŁ (Bezchmurnie)
        // off = -6 -> Podstawa (góra trójkąta): 24 + (-6) = 18 (IDEALNIE RÓWNO Z TEKSTEM)
        int off = -6; 
        tft.fillTriangle(x, y + h + off, x - w/2, y + off, x + w/2, y + off, color);
    }
    else if (direction == -2) { // PODWÓJNY DÓŁ
        int off = -6;
        tft.fillTriangle(x, y + off, x - w/2, y - h + off, x + w/2, y - h + off, color);
        tft.fillTriangle(x, y + h + off, x - w/2, y + off, x + w/2, y + off, color);
    }
    else { // KRESKA
        // Rysujemy na środku wysokości tekstu (ok. y=22)
        tft.fillRect(x - 3, y - 2, 6, 3, color); 
    }
}

// ================================================================
// LOGIKA TRENDU CIŚNIENIA - FINALNA (REALNE DANE + HISTORIA 3H)
// ================================================================

void getPressureTrendInfo(float currentPressure, String &trendText, String &forecastText, uint16_t &trendColor, int &trendDirection) {
    // --- KONFIGURACJA ---
    const int HISTORY_SIZE = 13;       // 12 slotów po 15 min = 3h (+1 na bieżący)
    const unsigned long INTERVAL = 15 * 60 * 1000UL; // Co 15 minut zapisujemy punkt
    
    // Zmienne statyczne (pamiętają wartość między wywołaniami funkcji)
    static float history[HISTORY_SIZE] = {0}; 
    static bool isInitialized = false;
    static unsigned long lastUpdate = 0;
    static int count = 0; // Ile mamy próbek

    // 1. INICJALIZACJA
    if (!isInitialized || history[0] == 0) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            history[i] = currentPressure;
        }
        isInitialized = true;
        lastUpdate = millis();
        count = 1;
    }

    // 2. AKTUALIZACJA HISTORII (Co 15 minut)
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

    // 4. USTALANIE STATUSU (ZAMBRETTI)
    float thresholdStable = 0.5; // +/- 0.5 hPa uznajemy za stałe
    float thresholdStorm = 4.0;  // Spadek o 4 hPa to gwałtowna zmiana

    // Logika mapowania trendu na opisy i strzałki (zgodna z Twoim UI)
    
    if (diff > thresholdStorm) {
        // Gwałtownie rośnie -> Wyż, czyste niebo
        trendText = "Szybko rosnie";
        forecastText = "Wiatr Bezchm.";
        trendColor = TFT_MAGENTA;
        trendDirection = -1; // Strzałka w dół (uspokojenie/brak chmur)
        
    } else if (diff > thresholdStable) {
        // Rośnie -> Poprawa pogody
        trendText = "Rosnie";
        forecastText = "Wiatr Mniej chm.";
        trendColor = TFT_GREEN;
        trendDirection = -1; // Strzałka w dół
        
    } else if (diff < -thresholdStorm) {
        // Gwałtownie spada -> Burza / Wichura
        trendText = "Gwaltownie";
        forecastText = "Wiatr BURZA!";
        trendColor = TFT_RED;
        trendDirection = 2; // Podwójna strzałka w górę
        
    } else if (diff < -thresholdStable) {
        // Spada -> Pogorszenie
        trendText = "Spada";
        forecastText = "Wiatr Pochm.";
        trendColor = TFT_ORANGE;
        trendDirection = 1; // Strzałka w górę
        
    } else {
        // Stabilnie
        trendText = "Stabilne";
        forecastText = "Bez zmian";
        trendColor = TFT_CYAN;
        trendDirection = 0; // Kreska
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
    int trendDir; 
    getPressureTrendInfo(weather.pressure, trendTxt, forecastTxt, trendCol, trendDir);

    // 1. Rysujemy strzałkę
    // BAZA: y3 + 24 (Strzałki w dół są podnoszone wewnątrz funkcji drawTrendArrow o 6 pikseli)
    drawTrendArrow(tft, rightX + 12, y3 + 24, trendDir, trendCol);

    // 2. Napisy (CIS + Trend)
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("CIS.", rightX + 5, y3 + 5);

    tft.setTextColor(trendCol, TEXT_BG);
    tft.drawString(trendTxt, rightX + 35, y3 + 5);

    // --- C. DOLNA SEKCJA (WIATR + POGODA) ---
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    tft.setTextSize(1);
    
    int textXOffset = 22; 
    int spaceIndex = forecastTxt.indexOf(' ');
    
    if (spaceIndex > 0) {
        String line1 = forecastTxt.substring(0, spaceIndex); 
        String line2 = forecastTxt.substring(spaceIndex + 1); 
        
        // Linia 1: "Wiatr" (Podniesione o 2px: było 20 -> jest 18)
        tft.drawString(line1, rightX + textXOffset, y3 + 18);
        
        // Linia 2: "Pogoda" (Podniesione o 2px: było 30 -> jest 28)
        tft.drawString(line2, rightX + textXOffset, y3 + 28);
    } else {
        // Jedno słowo (Podniesione o 2px: było 24 -> jest 22)
        tft.drawString(forecastTxt, rightX + textXOffset, y3 + 22);
    }
    
    // D. Wartość liczbowa
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(getPressureColor(weather.pressure), TEXT_BG);
    tft.setTextSize(2);
    tft.drawString(String((int)weather.pressure), rightX + rightW - 30, y3 + 20); 

    // E. Jednostka
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