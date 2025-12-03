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
#include "config/location_config.h"

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
  
  // Sprawdź wiek danych (informacyjnie)
  unsigned long dataAge = millis() - weeklyForecast.lastUpdate;
  Serial.printf("📊 Weekly data: valid=%s, count=%d, age=%.1fh\n", 
               weeklyForecast.isValid ? "YES" : "NO", weeklyForecast.count, dataAge / 3600000.0);
  
  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    // Pokaz error screen dla brakujących danych
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak danych", 160, 80);
    tft.setTextSize(1);
    tft.drawString("prognoza tygodniowa", 160, 110);
    tft.drawString("Wpisz 'x' w Serial", 160, 130);
    Serial.println("❌ Weekly data not available");
    return;
  }
  
  // === RENDERUJ DANE WEEKLY ===
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  // DYNAMICZNE WYRÓWNANIE - dostosuj wysokość do liczby dni
  int availableHeight = 200; // Wysokość ekranu minus margines
  int startY = 15;
  int rowHeight = weeklyForecast.count > 0 ? (availableHeight / weeklyForecast.count) : 35;
  
  Serial.printf("📐 Wyrównanie ekranu: %d dni, wysokość rzędu: %dpx\n", 
                weeklyForecast.count, rowHeight);
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    DailyForecast& day = weeklyForecast.days[i];
    int y = startY + (i * rowHeight);
    
    // === NAZWA DNIA ===
    tft.setTextColor(TFT_WHITE); // Bialy
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(day.dayName, 10, y);
    
    // === PRAWDZIWE IKONY POGODOWE (jak na ekranie 1 i 2) ===
    extern void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);
    
    int iconX = 55;
    int iconY = y - (rowHeight / 4); // Dynamicznie wycentruj ikonę
    
    // Mapowanie ikony na warunek pogodowy (dla drawWeatherIcon)
    String condition = "";
    if (day.icon.indexOf("01") >= 0) condition = "clear sky";
    else if (day.icon.indexOf("02") >= 0) condition = "few clouds";
    else if (day.icon.indexOf("03") >= 0) condition = "scattered clouds";
    else if (day.icon.indexOf("04") >= 0) condition = "overcast clouds";
    else if (day.icon.indexOf("09") >= 0) condition = "shower rain";
    else if (day.icon.indexOf("10") >= 0) condition = "rain";
    else if (day.icon.indexOf("11") >= 0) condition = "thunderstorm";
    else if (day.icon.indexOf("13") >= 0) condition = "snow";
    else if (day.icon.indexOf("50") >= 0) condition = "mist";
    else condition = "unknown";
    
    // Rysuj prawdziwa ikone pogodowa (25x25 px)
    // Tymczasowo skaluj ikone dla weekly
    int originalIconSize = 50; // ICON_SIZE z config
    
    // Przeskaluj ikone: oryginal 50x50 -> 25x25 dla weekly
    // Rysuj w połowie rozmiaru
    extern void drawWeatherIconScaled(TFT_eSPI& tft, int x, int y, String condition, String iconCode, float scale);
    
    // Jesli funkcja scalowana nie istnieje, uzyj oryginalnej z przeskalowaniem
    drawWeatherIcon(tft, iconX, iconY, condition, day.icon);
    
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
    tft.setTextDatum(TL_DATUM);
    
    int windY = y + (rowHeight / 3); // Dynamiczne pozycjonowanie wiatru
    
    // Min wiatr (ciemnoszary)
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(String((int)day.windMin) + "-", 220, windY);
    
    // Max wiatr (bialy)  
    tft.setTextColor(TFT_WHITE);
    tft.drawString(String((int)day.windMax) + "km/h", 240, windY);
    
    // === OPADY (zawsze pokazuj na ciemno niebiesko) ===
    tft.setTextColor(0x001F); // Ciemny niebieski
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String(day.precipitationChance) + "%", 285, windY);
    
    Serial.printf("📅 Rendered: %s, %.1f'-%.1f'C, %s\n", 
                  day.dayName.c_str(), day.tempMin, day.tempMax, day.icon.c_str());
  }
  
  // === FOOTER z lokalizacją i czasem aktualizacji ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  
  // Obecna lokalizacja z dzielnicą (lewa strona)
  extern LocationManager locationManager;
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  tft.setTextDatum(TL_DATUM);
  String locationText = String(currentLoc.cityName) + ", " + String(currentLoc.displayName);
  tft.drawString(locationText, 10, 210);
  
  // Czas ostatniej aktualizacji z inteligentnym formatem (prawa strona)  
  unsigned long ageSeconds = (millis() - weeklyForecast.lastUpdate) / 1000;
  String updateText;
  
  if (ageSeconds < 60) {
    updateText = "Aktualizacja: " + String(ageSeconds) + "s temu";
  } else if (ageSeconds < 3600) {
    unsigned long ageMinutes = ageSeconds / 60;
    updateText = "Aktualizacja: " + String(ageMinutes) + "min temu";
  } else {
    unsigned long ageHours = ageSeconds / 3600;
    updateText = "Aktualizacja: " + String(ageHours) + "h temu";
  }
  
  tft.setTextDatum(TR_DATUM);
  tft.drawString(updateText, 310, 210);
  
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