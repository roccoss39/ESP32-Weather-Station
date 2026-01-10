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

// Helper to determine pressure trend string and color
// You will need to replace the logic inside with your actual trend calculation
// ================================================================
// LOGIKA TRENDU CIŚNIENIA (Z PAMIĘCIĄ 3H)
// ================================================================

void getPressureTrendInfo(float currentPressure, String &trendText, String &forecastText, uint16_t &trendColor) {
    // --- KONFIGURACJA ---
    const int HISTORY_SIZE = 13;       // 12 slotów po 15 min = 3h (+1 na bieżący)
    const unsigned long INTERVAL = 15 * 60 * 1000UL; // Co 15 minut zapisujemy punkt
    
    // Zmienne statyczne (pamiętają wartość między wywołaniami funkcji)
    static float history[HISTORY_SIZE] = {0}; 
    static bool isInitialized = false;
    static unsigned long lastUpdate = 0;
    static int count = 0; // Ile mamy próbek

    // 1. INICJALIZACJA (przy pierwszym uruchomieniu wypełnij tablicę obecnym ciśnieniem)
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
        
        // Przesuń historię (najstarsze wypada, robimy miejsce na nowe na początku)
        for (int i = HISTORY_SIZE - 1; i > 0; i--) {
            history[i] = history[i - 1];
        }
        history[0] = currentPressure; // Zapisz nowe na pozycji 0
        
        if (count < HISTORY_SIZE) count++; // Wiemy, że mamy więcej prawdziwych danych
    }
    
    // Aktualizuj bieżący odczyt na żywo (bez przesuwania historii)
    // Żeby porównywać "Teraz" vs "3h temu", a nie "15 min temu" vs "3h temu"
    // Ale w uproszczeniu porównujemy history[0] z history[last]
    
    // 3. OBLICZANIE RÓŻNICY (Tendencja)
    // Porównujemy najnowszy (index 0) z najstarszym dostępnym (index count-1)
    // Docelowo history[12] to odczyt sprzed 3h.
    float pressure3hAgo = history[count - 1]; 
    float diff = currentPressure - pressure3hAgo;

    // 4. USTALANIE STATUSU (ZAMBRETTI - UPROSZCZONY)
    
    // Progi czułości (hPa na 3h)
    float thresholdStable = 0.5; // +/- 0.5 hPa uznajemy za stałe
    float thresholdStorm = 4.0;  // Spadek o 4 hPa to gwałtowna zmiana

    // A. Określanie tekstu trendu
    int trendState = 0; // 0=Stable, 1=Rise, 2=Fall, 3=FallFast, 4=RiseFast

    if (diff > thresholdStorm) {
        trendText = "Szybko rosnie";
        trendColor = TFT_GREEN;
        trendState = 4;
    } else if (diff > thresholdStable) {
        trendText = "Rosnie";
        trendColor = TFT_GREEN;
        trendState = 1;
    } else if (diff < -thresholdStorm) {
        trendText = "Gw. spada!";
        trendColor = TFT_RED;
        trendState = 3;
    } else if (diff < -thresholdStable) {
        trendText = "Spada";
        trendColor = TFT_ORANGE;
        trendState = 2;
    } else {
        trendText = "Stabilne";
        trendColor = TFT_CYAN;
        trendState = 0;
    }

    // B. Prognozowanie (Logika uproszczona Zambrettiego)
    // Łączymy aktualne ciśnienie z tendencją
    
    if (trendState == 0) { // STABILNE
        if (currentPressure > 1020) forecastText = "Ladna pogoda";
        else if (currentPressure > 1010) forecastText = "Zmienne zachm.";
        else forecastText = "Mozliwy deszcz";
    }
    else if (trendState == 1 || trendState == 4) { // ROŚNIE
        if (currentPressure > 1010) forecastText = "Bedzie slonce";
        else forecastText = "Poprawa pogody";
    }
    else if (trendState == 2) { // SPADA
        if (currentPressure > 1015) forecastText = "Zachmurzenie";
        else if (currentPressure > 1000) forecastText = "Bedzie padac";
        else forecastText = "Deszcz/Wiatr";
    }
    else if (trendState == 3) { // GWAŁTOWNIE SPADA
        forecastText = "BURZA/WIATR!";
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

    // --- 3. CIŚNIENIE (POWRÓT DO ORYGINAŁU) ---
   uint8_t y3 = y2 + rowH + gap;
    tft.fillRoundRect(rightX, y3, rightW, rowH, 6, CARD_BG);
    tft.drawRoundRect(rightX, y3, rightW, rowH, 6, BORDER_COLOR);
    
    // Pobranie danych trendu
    String trendTxt, forecastTxt;
    uint16_t trendCol;
    getPressureTrendInfo(weather.pressure, trendTxt, forecastTxt, trendCol);

    // A. Etykieta "CIS." (Lewa Góra)
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("CIS.", rightX + 5, y3 + 5);

    // B. Trend obok etykiety (np. "Spada") - Lewa Góra, obok CIS.
    tft.setTextColor(trendCol, TEXT_BG);
    tft.drawString(trendTxt, rightX + 35, y3 + 5);

    // C. Prognoza (LOGIKA ŁAMANIA TEKSTU)
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    tft.setTextSize(1); // Mała czcionka dla opisów
    
    // Sprawdzamy czy w tekście jest spacja (np. "Mozliwy deszcz")
    int spaceIndex = forecastTxt.indexOf(' ');
    
    if (spaceIndex > 0) {
        // --- PRZYPADEK 1: Dwa słowa (lub więcej) ---
        String line1 = forecastTxt.substring(0, spaceIndex); // "Mozliwy"
        String line2 = forecastTxt.substring(spaceIndex + 1); // "deszcz"
        
        // Linia 1: Podniesiona do góry (zaraz pod "CIS.")
        tft.drawString(line1, rightX + 5, y3 + 17);
        
        // Linia 2: Na dole
        tft.drawString(line2, rightX + 5, y3 + 27);
    } else {
        // --- PRZYPADEK 2: Jedno słowo (np. "Burza") ---
        // Rysujemy pośrodku dostępnego miejsca w pionie
        tft.drawString(forecastTxt, rightX + 5, y3 + 22);
    }
    
    // D. WARTOŚĆ LICZBOWA (Czcionka 2 - zgodnie z Twoim życzeniem)
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(getPressureColor(weather.pressure), TEXT_BG);
    tft.setTextSize(2); // Zmienione na 2
    
    // y3 + 20 to środek wysokości karty, przy czcionce 2 będzie wyglądać ok
    tft.drawString(String((int)weather.pressure), rightX + rightW - 30, y3 + 20); 

    // E. Jednostka
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    // Dostosowana pozycja jednostki do mniejszej liczby
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