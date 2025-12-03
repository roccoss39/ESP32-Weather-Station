#include "managers/ScreenManager.h"
#include "display/screen_manager.h"
#include "display/weather_display.h"
#include "display/forecast_display.h"
#include "display/time_display.h"
#include "display/github_image.h"
#include "config/display_config.h"
#include "sensors/motion_sensor.h"
#include "managers/WeatherCache.h"
#include "managers/TimeDisplayCache.h"

// Singleton instance ScreenManager
static ScreenManager screenManager;

ScreenManager& getScreenManager() {
  return screenManager;
}

// ❌ USUNIĘTE: 3 extern variables zastąpione ScreenManager class

void updateScreenManager() {
  // Nie przełączaj ekranów jeśli display śpi
  if (getDisplayState() == DISPLAY_SLEEPING) {
    return;
  }
  
  // Deleguj do ScreenManager - OOP style
  getScreenManager().updateScreenManager();
}

void switchToNextScreen(TFT_eSPI& tft) {
  // Deleguj do ScreenManager - OOP style
  getScreenManager().renderCurrentScreen(tft);
}

void forceScreenRefresh(TFT_eSPI& tft) {
  // Deleguj do ScreenManager - OOP style
  getScreenManager().forceScreenRefresh(tft);
}

// --- IMPLEMENTACJA RENDERING METHODS dla ScreenManager ---

void ScreenManager::renderWeatherScreen(TFT_eSPI& tft) {
  // Ekran 1: Aktualna pogoda + czas
  displayWeather(tft);
  displayTime(tft);
}

void ScreenManager::renderForecastScreen(TFT_eSPI& tft) {
  // Ekran 2: Prognoza 3h
  displayForecast(tft);
}

void ScreenManager::renderWeeklyScreen(TFT_eSPI& tft) {
  // Ekran 3: WEEKLY - prognoza 5-dniowa
  Serial.println("📱 Ekran wyczyszczony - rysowanie: WEEKLY");
  
  // Sprawdz czy dane weekly sa dostepne
  extern WeeklyForecastData weeklyForecast;
  
  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    // Pokaz error screen
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak danych", 160, 80);
    tft.setTextSize(1);
    tft.drawString("weekly forecast", 160, 110);
    tft.drawString("Wpisz 'weekly' w Serial", 160, 130);
    Serial.println("❌ Weekly data not available");
    return;
  }
  
  // === RENDERUJ DANE WEEKLY ===
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  int startY = 15;
  int rowHeight = 35;
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    DailyForecast& day = weeklyForecast.days[i];
    int y = startY + (i * rowHeight);
    
    // === NAZWA DNIA ===
    tft.setTextColor(TFT_WHITE); // Bialy
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(day.dayName, 10, y);
    
    // === IKONA POGODY (proste kolo) ===
    uint16_t iconColor = TFT_WHITE;
    if (day.icon.indexOf("01") >= 0) iconColor = 0xFFE0;      // Slonce - zolty
    else if (day.icon.indexOf("02") >= 0) iconColor = 0xCE79; // Czesciowe chmury
    else if (day.icon.indexOf("04") >= 0) iconColor = 0x7BEF; // Chmury - szary
    else if (day.icon.indexOf("09") >= 0) iconColor = 0x07FF; // Deszcz - cyan
    else if (day.icon.indexOf("11") >= 0) iconColor = 0xF81F; // Burza - magenta
    else if (day.icon.indexOf("13") >= 0) iconColor = TFT_WHITE; // Snieg
    
    tft.fillCircle(80, y + 10, 8, iconColor);
    
    // === TEMPERATURY MIN/MAX ===
    tft.setTextSize(2);
    
    // Min temp (szary)
    tft.setTextColor(TFT_LIGHTGREY); // Szary dla min
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String((int)day.tempMin) + "'", 120, y);
    
    // Separator
    tft.setTextColor(TFT_WHITE);
    tft.drawString("/", 155, y);
    
    // Max temp (bialy)
    tft.setTextColor(TFT_WHITE); // Bialy dla max
    tft.drawString(String((int)day.tempMax) + "'", 170, y);
    
    // === WIATR MIN/MAX ===
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextDatum(TL_DATUM);
    String windText = String((int)day.windMin) + "-" + String((int)day.windMax) + "km/h";
    tft.drawString(windText, 220, y + 8);
    
    // === OPADY (jesli > 20%) ===
    if (day.precipitationChance > 20) {
      tft.setTextColor(0x07E0); // Zielony
      tft.setTextSize(1);
      tft.drawString(String(day.precipitationChance) + "%", 280, y + 20);
    }
    
    Serial.printf("📅 Rendered: %s, %.1f'-%.1f'C, %s\n", 
                  day.dayName.c_str(), day.tempMin, day.tempMax, day.icon.c_str());
  }
  
  // === FOOTER ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  unsigned long age = (millis() - weeklyForecast.lastUpdate) / 1000;
  tft.drawString("Update: " + String(age) + "s ago", 160, 210);
  
  Serial.println("✅ Weekly screen rendered with weather data");
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  // Ekran 3: Zdjęcie z GitHub
  displayGitHubImage(tft);
}

void ScreenManager::resetWeatherAndTimeCache() {
  // Coordination z Phase 1+2 managers - teraz includes są w .cpp
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}