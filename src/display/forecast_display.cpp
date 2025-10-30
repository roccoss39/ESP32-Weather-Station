#include "display/forecast_display.h"
#include "display/weather_icons.h"
#include "config/display_config.h"
#include "weather/forecast_data.h"
#include "weather/weather_data.h"

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
  
  // Rysuj ikony, godziny i wiatr
  drawForecastItems(tft, 120);
  
  // Rysuj podsumowanie na dole (jedna linia)
  drawForecastSummary(tft, 215);
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
      tempStr = String(forecast.items[i].temperature, 0);
    } else {
      // Normalny format z 'C
      tempStr = String(forecast.items[i].temperature, 0) + "'C";
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
  
  // Temp: z małą czcionką
  tft.setTextSize(1);
  tft.drawString("Temp:", 10, y);
  
  // Wartości temperatury z normalną czcionką - dynamiczne formatowanie
  tft.setTextSize(2);
  String tempValues;
  
  // Sprawdź czy temperatura jest bardzo niska (≤ -5°C)
  if (minTemp <= -5.0 || maxTemp <= -5.0) {
    // Bez 'C dla bardzo niskich temperatur (oszczędność miejsca)
    tempValues = String(minTemp, 0) + "/" + String(maxTemp, 0);
    Serial.println("Uzywam skroconego formatu temperatury dla niskich wartosci: " + tempValues);
  } else {
    // Normalny format z 'C
    tempValues = String(minTemp, 0) + "'C/" + String(maxTemp, 0) + "'C";
  }
  
  tft.drawString(tempValues, 45, y);
  
  // Wiatr.max: z małą czcionką  
  tft.setTextSize(1);
  tft.drawString("Wiatr.max:", 160, y);
  
  // Wartości wiatru z normalną czcionką
  tft.setTextSize(2);
  String windValue = String(maxWind, 0) + "km/h";
  tft.drawString(windValue, 230, y);
  
  Serial.println("Podsumowanie: Temp:" + String(minTemp, 0) + "'C/" + String(maxTemp, 0) + 
                "'C Wiatr.max:" + String(maxWind, 0) + "km/h");
}