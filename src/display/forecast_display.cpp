#include "display/forecast_display.h"
#include "display/weather_icons.h"
#include "config/display_config.h"
#include "weather/forecast_data.h"
#include "weather/weather_data.h"

// formatTemperature moved to display_config.h

void displayForecast(TFT_eSPI& tft) {
  if (!forecast.isValid || forecast.count == 0) {
    // Brak danych prognozy - wyświetl komunikat
    tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak prognozy", tft.width() / 2, tft.height() / 2);
    return;
  }

  Serial.println("Wyświetlanie prognozy 3h...");

  // Rysuj wykres temperatury na górze
  drawTemperatureGraph(tft, 10, 20, 300, 80);
  
  // Rysuj ikony, godziny i wiatr (podniesione o 15px)
  drawForecastItems(tft, 105);
  
  // Rysuj podsumowanie na dole (podniesione o 15px)
  drawForecastSummary(tft, 200);
}

void drawTemperatureGraph(TFT_eSPI& tft, int x, int y, int width, int height) {
  if (forecast.count < 2) return;
  
  // Znajdź min/max temperaturę dla skalowania
  float minTemp = forecast.items[0].temperature;
  float maxTemp = forecast.items[0].temperature;
  
  for (int i = 1; i < forecast.count; i++) {
    if (forecast.items[i].temperature < minTemp) minTemp = forecast.items[i].temperature;
    if (forecast.items[i].temperature > maxTemp) maxTemp = forecast.items[i].temperature;
  }
  
  // Dodaj margines do skali
  float tempRange = maxTemp - minTemp;
  if (tempRange < 5) tempRange = 5; // Minimum 5°C range
  minTemp -= 1;
  maxTemp += 1;
  tempRange = maxTemp - minTemp;
  
  Serial.println("Wykres temperatury: " + String(minTemp, 1) + "°C do " + String(maxTemp, 1) + "°C");
  
  // Rysuj ramkę wykresu
  tft.drawRect(x, y, width, height, TFT_WHITE);
  
  // Oblicz pozycje punktów
  int pointX[5];
  int pointY[5];
  
  for (int i = 0; i < forecast.count; i++) {
    pointX[i] = x + 20 + (i * (width - 40) / (forecast.count - 1));
    pointY[i] = y + height - 10 - ((forecast.items[i].temperature - minTemp) / tempRange) * (height - 20);
  }
  
  // Rysuj linie między punktami
  for (int i = 0; i < forecast.count - 1; i++) {
    tft.drawLine(pointX[i], pointY[i], pointX[i+1], pointY[i+1], TFT_ORANGE);
  }
  
  // Rysuj punkty i temperatury
  for (int i = 0; i < forecast.count; i++) {
    // Punkt na wykresie
    tft.fillCircle(pointX[i], pointY[i], 3, TFT_ORANGE);
    
    // Temperatura nad punktem - dynamiczne formatowanie
    tft.setTextColor(TFT_ORANGE, COLOR_BACKGROUND);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    
    // === POPRAWKA NA -0 ===
    float tempVal = forecast.items[i].temperature;
    if (round(tempVal) == 0) tempVal = 0; // Jeśli zaokrąglenie to 0, usuwamy minus
    // ======================

    String tempStr;
    if (tempVal <= -5.0) {
      // Bez 'C dla bardzo niskich temperatur
      tempStr = formatTemperature(tempVal, 0);
    } else {
      // Normalny format z 'C
      tempStr = formatTemperature(tempVal, 0) + "'C";
    }
    
    tft.drawString(tempStr, pointX[i], pointY[i] - 15);
  }
}

void drawForecastItems(TFT_eSPI& tft, int startY) {
  if (forecast.count == 0) return;
  
  int itemWidth = 320 / forecast.count;  // Szerokość na każdy element
  uint8_t iconSize = 40;                     // Rozmiar ikony
  
  for (int i = 0; i < forecast.count; i++) {
    int centerX = (i * itemWidth) + (itemWidth / 2);
    
    // Ikona pogody
    int iconX = centerX - (iconSize / 2);
    int iconY = startY;
    drawWeatherIcon(tft, iconX, iconY, forecast.items[i].description, forecast.items[i].icon);
    
    // Godzina pod ikoną (bez dwukropka)
    String timeStr = forecast.items[i].time;
    timeStr.replace(":", "");  // Usuń dwukropek
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(timeStr, centerX, startY + iconSize + 5);
    
    // Prędkość wiatru pod godziną (km/h)
    float windKmh = forecast.items[i].windSpeed * 3.6;
    tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
    tft.setTextSize(1);
    String windStr = String(windKmh, 0) + "km/h";
    tft.drawString(windStr, centerX, startY + iconSize + 25);
    
    // Debug info
    Serial.println("Item " + String(i+1) + ": " + forecast.items[i].time + 
                  " " + String(forecast.items[i].temperature, 1) + "°C " + 
                  String(windKmh, 0) + "km/h " + forecast.items[i].icon);
  }
}

void drawForecastSummary(TFT_eSPI& tft, int y) {
  if (forecast.count == 0) return;
  
  // Oblicz min/max temperatury
  float minTemp = forecast.items[0].temperature;
  float maxTemp = forecast.items[0].temperature;
  float maxWind = 0;
  
  for (int i = 0; i < forecast.count; i++) {
    if (forecast.items[i].temperature < minTemp) minTemp = forecast.items[i].temperature;
    if (forecast.items[i].temperature > maxTemp) maxTemp = forecast.items[i].temperature;
    
    float windKmh = forecast.items[i].windSpeed * 3.6;
    if (windKmh > maxWind) maxWind = windKmh;
  }

  // === POPRAWKA NA -0 DLA SUMMARY ===
  if (round(minTemp) == 0) minTemp = 0;
  if (round(maxTemp) == 0) maxTemp = 0;
  // ==================================
  
  // Styl tekstu - mniejsza czcionka tylko dla nazw T. i W.max
  tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
  tft.setTextDatum(TL_DATUM);
  
  // === SEKCJA 1: TEMP i WIATR ===
  // Bazowy Y dla wartości (duża czcionka) to: y - 10
  // Bazowy Y dla etykiet (mała czcionka) przesuwamy w dół o 5px: y - 5
  
  // Temp: z małą czcionką - wyśrodkowane w pionie względem wartości
  tft.setTextSize(1);
  tft.drawString("TEMP:", 10, y - 5); // Zmiana z -10 na -5 (obniżenie)
  
  // Wartości temperatury z normalną czcionką
  tft.setTextSize(2);
  
  // Sprawdź czy temperatura jest bardzo niska (≤ -5°C)
  if (minTemp <= -5.0 || maxTemp <= -5.0) {
    String minTempStr = formatTemperature(minTemp, 0);
    String maxTempStr = formatTemperature(maxTemp, 0);
    
    // Min temp (szary)
    tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
    tft.drawString(minTempStr + "/", 45, y - 10);
    
    // Max temp (biały)
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.drawString(maxTempStr, 45 + (minTempStr.length() * 12) + 7, y - 10);
    
  } else {
    String minTempStr = formatTemperature(minTemp, 0) + "'C";
    String maxTempStr = formatTemperature(maxTemp, 0) + "'C";
    
    // Min temp (szary)
    tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
    tft.drawString(minTempStr + "/", 45, y - 10);
    
    // Max temp (biały)
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.drawString(maxTempStr, 45 + (minTempStr.length() * 12) + 6, y - 10);
  }
  
  // Wiatr.max: z małą czcionką - wyśrodkowane w pionie względem wartości
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
  tft.drawString("WIATR.MAX:", 160, y - 5); // Zmiana z -10 na -5 (obniżenie)
  
  // Wartości wiatru z normalną czcionką
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
  String windValue = String(maxWind, 0) + "km/h";
  tft.drawString(windValue, 230, y - 10);
  
  
  // === SEKCJA 2: WSCHÓD i ZACHÓD ===
  
  if (weather.sunrise > 0 && weather.sunset > 0) {
    unsigned long sunriseLocal = weather.sunrise + 3600; // UTC+1
    unsigned long sunsetLocal = weather.sunset + 3600;
    
    uint8_t sunriseHour = (sunriseLocal % 86400) / 3600;
    uint8_t sunriseMin = (sunriseLocal % 3600) / 60;
    uint8_t sunsetHour = (sunsetLocal % 86400) / 3600;
    uint8_t sunsetMin = (sunsetLocal % 3600) / 60;
    
    String sunriseTime = (sunriseHour < 10 ? "0" : "") + String(sunriseHour) + ":" + (sunriseMin < 10 ? "0" : "") + String(sunriseMin);
    String sunsetTime = (sunsetHour < 10 ? "0" : "") + String(sunsetHour) + ":" + (sunsetMin < 10 ? "0" : "") + String(sunsetMin);
    
    // Bazowy Y dla wartości (duża czcionka): y + 12
    // Bazowy Y dla etykiet (mała czcionka): y + 17 (obniżenie o 5px względem wartości)
    
    // Wschód słońca - etykieta
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, COLOR_BACKGROUND);
    tft.drawString("WSCHOD: ", 10, y + 17); // Zmiana z 15 na 17 (wyśrodkowanie względem dużej czcionki)
    
    // Godzina wschodu - wartość
    tft.setTextSize(2);
    tft.drawString(sunriseTime, 60, y + 12); 
    
    // Zachód słońca - etykieta
    tft.setTextSize(1);
    tft.drawString("ZACHOD: ", 160, y + 17); // Zmiana z 15 na 17
    
    // Godzina zachodu - wartość
    tft.setTextSize(2);
    tft.drawString(sunsetTime, 210, y + 12);
  }
}