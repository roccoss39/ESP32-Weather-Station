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
    
    String tempStr;
    if (forecast.items[i].temperature <= -5.0) {
      // Bez 'C dla bardzo niskich temperatur
      tempStr = formatTemperature(forecast.items[i].temperature, 0);
    } else {
      // Normalny format z 'C
      tempStr = formatTemperature(forecast.items[i].temperature, 0) + "'C";
    }
    
    tft.drawString(tempStr, pointX[i], pointY[i] - 15);
  }
}

void drawForecastItems(TFT_eSPI& tft, int startY) {
  if (forecast.count == 0) return;
  
  int itemWidth = 320 / forecast.count;  // Szerokość na każdy element
  int iconSize = 40;                     // Rozmiar ikony
  
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
  
  // Styl tekstu - mniejsza czcionka tylko dla nazw T. i W.max
  tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
  tft.setTextDatum(TL_DATUM);
  
  // Temp: z małą czcionką - przesunięty o 10 pikseli wyżej
  tft.setTextSize(1);
  tft.drawString("TEMP:", 10, y - 10);
  
  // Wartości temperatury z normalną czcionką - dynamiczne formatowanie
  tft.setTextSize(2);
  String tempValues;
  
  // Sprawdź czy temperatura jest bardzo niska (≤ -5°C)
  if (minTemp <= -5.0 || maxTemp <= -5.0) {
    // Bez 'C dla bardzo niskich temperatur (oszczędność miejsca)
    String minTempStr = formatTemperature(minTemp, 0);
    String maxTempStr = formatTemperature(maxTemp, 0);
    
    // Min temp (szary)
    tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
    tft.drawString(minTempStr + "/", 45, y - 10);
    
    // Max temp (biały)
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.drawString(maxTempStr, 45 + (minTempStr.length() * 12) + 6, y - 10);
    
    Serial.println("Uzywam skroconego formatu temperatury dla niskich wartosci: " + minTempStr + "/" + maxTempStr);
  } else {
    // Normalny format z 'C - min temp (szary)
    String minTempStr = formatTemperature(minTemp, 0) + "'C";
    String maxTempStr = formatTemperature(maxTemp, 0) + "'C";
    
    // Min temp (szary)
    tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
    tft.drawString(minTempStr + "/", 45, y - 10);
    
    // Max temp (biały)
    tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    tft.drawString(maxTempStr, 45 + (minTempStr.length() * 12) + 6, y - 10);
  }
  
  // Wiatr.max: z małą czcionką - przesunięty o 10 pikseli wyżej
  tft.setTextSize(1);
  tft.setTextColor(TFT_LIGHTGREY, COLOR_BACKGROUND);
  tft.drawString("WIATR.MAX:", 160, y - 10);
  
  // Wartości wiatru z normalną czcionką (biały dla max wartości)
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
  String windValue = String(maxWind, 0) + "km/h";
  tft.drawString(windValue, 230, y - 10);
  
  // Dodaj wschód i zachód słońca pod temperaturą i wiatrem - przesunięty o 10 pikseli niżej
  if (weather.sunrise > 0 && weather.sunset > 0) {
    // Konwertuj Unix timestamp na czas lokalny (UTC+1 dla Polski)
    unsigned long sunriseLocal = weather.sunrise + 3600; // +1 godzina dla UTC+1
    unsigned long sunsetLocal = weather.sunset + 3600;
    
    // Oblicz godziny i minuty
    int sunriseHour = (sunriseLocal % 86400) / 3600;
    int sunriseMin = (sunriseLocal % 3600) / 60;
    int sunsetHour = (sunsetLocal % 86400) / 3600;
    int sunsetMin = (sunsetLocal % 3600) / 60;
    
    // Formatuj czas (HH:MM)
    // Format 4-cyfrowy: 07:06 zamiast 7:06
    String sunriseTime = (sunriseHour < 10 ? "0" : "") + String(sunriseHour) + ":" + (sunriseMin < 10 ? "0" : "") + String(sunriseMin);
    String sunsetTime = (sunsetHour < 10 ? "0" : "") + String(sunsetHour) + ":" + (sunsetMin < 10 ? "0" : "") + String(sunsetMin);
    
    // Wschód słońca - etykiety mała, godziny większa czcionka (obniżone o 5px)
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, COLOR_BACKGROUND);
    tft.drawString("WSCHOD: ", 10, y + 15);
    
    // Godzina wschodu - większa czcionka (obniżona o 5px)
    tft.setTextSize(2);
    tft.drawString(sunriseTime, 60, y + 15);
    
    // Zachód słońca - etykieta mała czcionka (obniżona o 5px)
    tft.setTextSize(1);
    tft.drawString("ZACHOD: ", 160, y + 15);
    
    // Godzina zachodu - większa czcionka (obniżona o 5px)
    tft.setTextSize(2);
    tft.drawString(sunsetTime, 210, y + 15);
    
    Serial.println("Słońce: Wschód " + sunriseTime + ", Zachód " + sunsetTime);
  }
  
  Serial.println("Podsumowanie: Temp:" + String(minTemp, 0) + "'C/" + String(maxTemp, 0) + 
                "'C Wiatr.max:" + String(maxWind, 0) + "km/h");
}