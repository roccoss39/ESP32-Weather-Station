#include "display/weekly_forecast_display.h"
#include "config/display_config.h"
#include "config/location_config.h"
#include "weather/forecast_data.h"
#include "display/display_utils.h"

// Zakomentowano dla normalnego działania (dodaj odkomentuj #define aby przetestować maksymalne wartości)
// #define ENABLE_WEEKLY_MOCK_TEST 1

extern WeeklyForecastData weeklyForecast;
extern LocationManager locationManager;

extern void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);

// ================================================================
// FUNKCJA WYŚWIETLANIA PROGNOZY TYGODNIOWEJ
// ================================================================

void displayWeeklyForecast(TFT_eSPI& tft) {
  Serial.println("📱 Ekran wyczyszczony - rysowanie: WEEKLY");
  
  // Zawsze upewniamy się, że zaczynamy od domyślnej czcionki
  tft.setFreeFont(NULL);
  tft.setTextFont(1);
  
  // Sprawdzenie danych
  extern bool isWeatherRefreshInProgress;
  extern unsigned long weatherRefreshStartMs;
  extern unsigned long weatherRefreshTimeoutMs;

  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    if (isWeatherRefreshInProgress && (millis() - weatherRefreshStartMs) < weatherRefreshTimeoutMs) {
      drawLoadingSpinner(tft, "Ladowanie tygodniowej...");
      return;
    }

    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak danych", 160, 80);
    tft.setTextSize(1);
    tft.drawString("prognoza tygodniowa", 160, 110);
    return;
  }
  
  // === STAŁE POZYCJE (KOTWICE) KOLUMN ===
  const int colTempX = 145;  // Mocno w prawo, ale przesunięte o 10px w lewo (było 155)
  const int colWindX = 220;  // Środek kolumny Wiatru przesunięty o kolejne 5px w lewo (było 225)
  const int colRainX = 315;  // Prawa krawędź kolumny Opadów
  // =====================================

  // === NAGŁÓWKI KOLUMN ===
  tft.setFreeFont(NULL);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_SILVER); // Srebrny, dyskretny kolor dla opisów

  tft.setTextDatum(TC_DATUM); // Wyrównanie do środka 
  tft.drawString("[Temp.]", colTempX, 3); 
  tft.drawString("[Wiatr km/h]", colWindX, 3);

  tft.setTextDatum(TR_DATUM); // Wyrównanie do prawej
  tft.drawString("[Opady]", colRainX, 3);
  
  tft.setTextDatum(TL_DATUM); // Reset do domyślnego wyrównania
  // =======================

  uint8_t availableHeight = 200; 
  uint8_t startY = 17; 
  uint8_t rowHeight = weeklyForecast.count > 0 ? (availableHeight / weeklyForecast.count) : 35;
  
  uint8_t textOffset = (rowHeight - 16) / 2; 
  if (textOffset < 0) textOffset = 0;
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    
    DailyForecast day = weeklyForecast.days[i]; 

#ifdef ENABLE_WEEKLY_MOCK_TEST
    // --- TRYB MOCK: Wymuszamy najszersze możliwe wartości dla poszczególnych dni ---
    switch (i) {
      case 0: day.tempMin = -22.0; day.tempMax = -10.0; day.windMin = 85.0;  day.windMax = 99.0;  day.precipitationChance = 100; break;
      case 1: day.tempMin = -99.0; day.tempMax = -15.0; day.windMin = 90.0;  day.windMax = 105.0; day.precipitationChance = 100; break;
      case 2: day.tempMin = -12.0; day.tempMax = -5.0;  day.windMin = 100.0; day.windMax = 120.0; day.precipitationChance = 99;  break;
      case 3: day.tempMin = -30.0; day.tempMax = 0.0;   day.windMin = 75.0;  day.windMax = 99.0;  day.precipitationChance = 100; break;
      case 4: day.tempMin = -45.0; day.tempMax = -20.0; day.windMin = 99.0;  day.windMax = 115.0; day.precipitationChance = 100; break;
    }
#endif

    int rawY = startY + (i * rowHeight);
    int y = rawY + textOffset + 4; // +4 dla wyrównania FreeFonts w pionie
    
    // USTAWIENIE GŁÓWNEJ CZCIONKI WEKTOROWEJ
    tft.setFreeFont(&FreeSans12pt7b);
    tft.setTextSize(1); 
    
    // 1. Dzień
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM); 
    String shortDay = day.dayName.substring(0, 3); 
    tft.drawString(shortDay, 5, y);
    
    // 2. Ikona (Przesunięta o 10px w lewo, na X=55)
    uint8_t iconX = 55;
    int iconY = rawY - (rowHeight / 4) + textOffset;
    
    String condition = "unknown";
    if (day.icon.indexOf("01") >= 0) condition = "clear sky";
    else if (day.icon.indexOf("02") >= 0) condition = "few clouds";
    else if (day.icon.indexOf("03") >= 0) condition = "scattered clouds";
    else if (day.icon.indexOf("04") >= 0) condition = "overcast clouds";
    else if (day.icon.indexOf("09") >= 0) condition = "shower rain";
    else if (day.icon.indexOf("10") >= 0) condition = "rain";
    else if (day.icon.indexOf("11") >= 0) condition = "thunderstorm";
    else if (day.icon.indexOf("13") >= 0) condition = "snow";
    else if (day.icon.indexOf("50") >= 0) condition = "mist";
    
    drawWeatherIcon(tft, iconX, iconY, condition, day.icon);
    
    // 3. Temperatury Min/Max - DYNAMICZNE ŚRODKOWANIE POD NAGŁÓWKIEM
    tft.setFreeFont(&FreeSans12pt7b);
    
    // Zabezpieczenie przed ujemnym zerem
    if (round(day.tempMin) == 0) day.tempMin = 0;
    if (round(day.tempMax) == 0) day.tempMax = 0;

    String minStr = String((int)round(day.tempMin));
    String maxStr = String((int)round(day.tempMax));
    
    // Obliczamy całkowitą szerokość bloku tekstu
    int tempTotalWidth = tft.textWidth(minStr) + tft.textWidth("/") + tft.textWidth(maxStr);
    
    // Zaczynamy rysować od środka kolumny minus połowa szerokości tekstu
    int tempStartX = colTempX - (tempTotalWidth / 2);  
    
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(minStr, tempStartX, y);
    tempStartX += tft.textWidth(minStr);  
    
    tft.setTextColor(TFT_WHITE);
    tft.drawString("/", tempStartX, y);
    tempStartX += tft.textWidth("/");  
    
    tft.drawString(maxStr, tempStartX, y);  
    
    // 4. Wiatr - DYNAMICZNE ŚRODKOWANIE POD NAGŁÓWKIEM
    String minWindStr = String((int)round(day.windMin));
    String maxWindStr = String((int)round(day.windMax));
    
    int windTotalWidth = tft.textWidth(minWindStr) + tft.textWidth("-") + tft.textWidth(maxWindStr);
    int windStartX = colWindX - (windTotalWidth / 2);
    
    // Własne, delikatne kolory dla wiatru
    uint16_t colorWindMin = tft.color565(150, 190, 220); 
    uint16_t colorWindMax = tft.color565(210, 235, 255); 

    tft.setTextColor(colorWindMin); 
    tft.drawString(minWindStr, windStartX, y);
    windStartX += tft.textWidth(minWindStr); 

    // Separator dla wiatru - na ciemnoszaro by nie odwracał uwagi od wartości
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("-", windStartX, y);
    windStartX += tft.textWidth("-");

    tft.setTextColor(colorWindMax);
    tft.drawString(maxWindStr, windStartX, y);

    // 5. Opady (Równanie do prawej krawędzi kolumny, ta sama wysokość Y)
    tft.setTextColor(0x001F); // Ciemny niebieski
    tft.setTextDatum(TR_DATUM); 
    tft.drawString(String(day.precipitationChance) + "%", colRainX, y); 
    tft.setTextDatum(TL_DATUM); 
  }
  
  // === FOOTER Z LOKALIZACJĄ ===
  tft.fillRect(0, 218, 320, 40, COLOR_BACKGROUND);
  tft.setFreeFont(&FreeSans12pt7b); 
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextDatum(TC_DATUM); // Top-Center
  
  if (locationManager.isLocationSet()) {
    WeatherLocation loc = locationManager.getCurrentLocation();
    String locationText;
    
    const bool hasCity = loc.cityName.length() > 0;
    const bool hasDisplay = loc.displayName.length() > 0;

    if (hasCity && hasDisplay && loc.displayName != loc.cityName) {
        locationText = loc.cityName + ", " + loc.displayName;
    } else if (hasDisplay) {
        locationText = loc.displayName;
    } else if (hasCity) {
        locationText = loc.cityName;
    } else {
        locationText = "";
    }

    // Zabezpieczenie długości - przy bardzo długich nazwach zmniejszamy na 9pt
    if (tft.textWidth(locationText) > 310) {
      tft.setFreeFont(&FreeSans9pt7b);
    }
    
    tft.drawString(locationText, 160, 222); 
    
  } else {
    tft.drawString("Brak lokalizacji", 160, 222);
  }

  tft.setFreeFont(NULL);
  tft.setTextFont(1);
}