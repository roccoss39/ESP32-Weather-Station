#include "display/weekly_forecast_display.h"
#include "config/display_config.h"
#include "config/location_config.h"
#include "weather/forecast_data.h"

// Extern dependencies
extern WeeklyForecastData weeklyForecast;
extern LocationManager locationManager;

// Extern funkcja drawWeatherIcon z weather_icons.cpp
extern void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);

// ================================================================
// FUNKCJA WYŚWIETLANIA PROGNOZY TYGODNIOWEJ
// ================================================================

void displayWeeklyForecast(TFT_eSPI& tft) {
  Serial.println("📱 Ekran wyczyszczony - rysowanie: WEEKLY");
  
  // Sprawdzenie danych
  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak danych", 160, 80);
    tft.setTextSize(1);
    tft.drawString("prognoza tygodniowa", 160, 110);
    return;
  }
  
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  uint8_t availableHeight = 200; 
  uint8_t startY = 15;
  uint8_t rowHeight = weeklyForecast.count > 0 ? (availableHeight / weeklyForecast.count) : 35;
  
  uint8_t textOffset = (rowHeight - 16) / 2; 
  if (textOffset < 0) textOffset = 0;
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    DailyForecast& day = weeklyForecast.days[i];
    int rawY = startY + (i * rowHeight);
    int y = rawY + textOffset; 
    
    // 1. Dzień
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(day.dayName, 10, y);
    
    // 2. Ikona
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
    
    // 3. Temperatury Min/Max - dynamiczne pozycjonowanie bez spacji
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    
    uint8_t tempX = 120;  // Start position
    
    // Min temp
    String minStr = String((int)round(day.tempMin));
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(minStr, tempX, y);
    tempX += tft.textWidth(minStr);  // Przesuń za tekst
    
    // Separator
    tft.setTextColor(TFT_WHITE);
    tft.drawString("/", tempX, y);
    tempX += tft.textWidth("/");  // Przesuń za separator
    
    // Max temp
    String maxStr = String((int)round(day.tempMax));
    tft.setTextColor(TFT_WHITE);
    tft.drawString(maxStr, tempX, y);
    tempX += tft.textWidth(maxStr);  // Oblicz koniec temperatur
    
    // 4. Skalowanie czcionki dla Wiatru/Opadów (uproszczone - bez warunku minusów)
    int8_t tMinInt = (int8_t)round(day.tempMin);
    int8_t tMaxInt = (int8_t)round(day.tempMax);
    
    uint8_t wideValuesCount = (abs(tMinInt) >= 10) + 
                          (abs(tMaxInt) >= 10) + 
                          (day.windMin >= 10) + 
                          (day.windMax >= 10) + 
                          (day.precipitationChance >= 10);

    // Jeśli 4 lub więcej wartości jest szerokich -> użyj małej czcionki
    bool useSmallFont = (wideValuesCount >= 4);

    uint8_t dataTextSize = useSmallFont ? 1 : 2;
    uint8_t yOffsetData = useSmallFont ? 5 : 0;
    uint8_t unitCorrection = useSmallFont ? 0 : 5; 

    // 5. Wiatr - dynamiczna pozycja PO temperaturach (z marginesem)
    tft.setTextSize(dataTextSize);
    int currentX = tempX + 10;  // Start 10px po końcu temperatur (dynamicznie!)

    int windY = y;
    if (useSmallFont)
    windY = y + yOffsetData + unitCorrection;

    tft.setTextColor(TFT_DARKGREY); 
    String minWind = String((int)round(day.windMin));
    tft.drawString(minWind, currentX, windY);
    currentX += tft.textWidth(minWind); 

    tft.setTextColor(TFT_WHITE);
    String sep = "-"; 
    tft.drawString(sep, currentX, windY);
    currentX += tft.textWidth(sep);

    String maxWind = String((int)round(day.windMax));
    tft.drawString(maxWind, currentX, windY);
    currentX += tft.textWidth(maxWind);

    tft.setTextSize(1);
    tft.setTextColor(TFT_SILVER);
    
    tft.drawString("km/h", currentX + 2, y + yOffsetData + unitCorrection);
    
    // 6. Opady - ZAWSZE normalna czcionka (size 2)
    tft.setTextColor(0x001F);
    tft.setTextDatum(TR_DATUM);
    if ((day.precipitationChance) >= 100)
    tft.setTextSize(1); 
    else   
    tft.setTextSize(2); 

    tft.drawString(String(day.precipitationChance) + "%", 315, y );
    tft.setTextDatum(TL_DATUM); 
  }
  
  // === FOOTER Z LOKALIZACJĄ ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM); // Punkt odniesienia: Środek Góry tekstu
  
  // Czyścimy dół ekranu (od Y=218 do końca)
  tft.fillRect(0, 218, 320, 40, COLOR_BACKGROUND);
  
  if (locationManager.isLocationSet()) {
    WeatherLocation loc = locationManager.getCurrentLocation();
    String locationText;
    
    // Budowanie nazwy
    if (loc.displayName.length() > 0 && loc.displayName != loc.cityName) {
        locationText = loc.cityName + ", " + loc.displayName;
    } else {
        locationText = loc.cityName;
    }

    // Zabezpieczenie długości
    int maxWidth = 310;
    if (tft.textWidth(locationText) > maxWidth) 
      tft.setTextSize(1);
    
    // Rysowanie
    tft.drawString(locationText, 160, 220); 
    
  } else {
    tft.drawString("Brak lokalizacji", 160, 220);
  }
}
