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
  
  // Obecna lokalizacja (lewa strona)
  tft.setTextDatum(TL_DATUM);
  
  // Wyczyść lewą stronę ekranu przed napisaniem
  tft.fillRect(0, 205, 150, 15, COLOR_BACKGROUND);
  
  // Lokalizacja - USING ACTUAL LOCATION MANAGER (properly declared)
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_DARKGREY);
  
  // Use the global locationManager that actually exists!
  if (locationManager.isLocationSet()) {
    WeatherLocation loc = locationManager.getCurrentLocation();
    String locationText = String(loc.cityName) + ", " + String(loc.displayName);
    
    // Truncate if too long (prevent artifacts)
    if (locationText.length() > 36) {
      locationText = locationText.substring(0, 33) + "...";
    }
    
    tft.drawString(locationText, 10, 210);
  } else {
    tft.drawString("Brak lokalizacji", 10, 210);
  }
  
  // Pusty footer po prawej stronie (aktualizacja przeniesiona na ekran 4)
  tft.setTextDatum(TR_DATUM);
  tft.fillRect(160, 205, 160, 15, COLOR_BACKGROUND);
  
  Serial.println("✅ Weekly screen rendered with weather data");
}

void ScreenManager::renderLocalSensorsScreen(TFT_eSPI& tft) {
  // Ekran 4: Lokalne czujniki DHT22
  Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS");
  
  tft.fillScreen(COLOR_BACKGROUND);
  
  // === NAGŁÓWEK ===
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("LOKALNE CZUJNIKI", 160, 10);
  
  // === INFORMACJE O DHT22 ===
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  
  // Temperatura
  tft.setCursor(20, 60);
  tft.print("Temp. lokalna: ");
  tft.setTextColor(TFT_YELLOW);
  tft.print("--.-'C");  // TODO: Dodać prawdziwe dane z DHT22
  
  // Wilgotność
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 110);
  tft.print("Wilg. lokalna: ");
  tft.setTextColor(TFT_CYAN);
  tft.print("--%");     // TODO: Dodać prawdziwe dane z DHT22
  
  // Status czujnika (mniejsza czcionka)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);  // ZMIENIONE: mniejsza czcionka
  tft.setCursor(20, 140);  // ZMIENIONE: wyżej
  tft.print("Status DHT22: ");
  tft.setTextColor(TFT_GREEN);
  tft.print("Gotowy");   // TODO: Dodać prawdziwe dane z DHT22
  
  // === FOOTER z informacjami o aktualizacjach ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  
  // Wyczyść obszar footera (więcej miejsca)
  tft.fillRect(0, 165, 320, 55, COLOR_BACKGROUND);  // ZMIENIONE: większy obszar
  
  // Etykieta AKTUALIZACJE (wyżej)
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("AKTUALIZACJE:", 160, 168);  // ZMIENIONE: wyżej
  
  // 1. Czujnik DHT22 (pierwszy rząd)
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Czujnik temp. i wilg.: co 2s", 10, 185);  // ZMIENIONE: nowa pozycja
  
  // 2. Pogoda bieżąca (drugi rząd)
  extern unsigned long lastWeatherCheckGlobal;
  unsigned long weatherAge = (millis() - lastWeatherCheckGlobal) / 1000;
  String weatherUpdateText;
  if (weatherAge < 60) {
    weatherUpdateText = "Pogoda: " + String(weatherAge) + "s temu (co 10min)";
  } else if (weatherAge < 3600) {
    unsigned long weatherMinutes = weatherAge / 60;
    weatherUpdateText = "Pogoda: " + String(weatherMinutes) + "min temu (co 10min)";
  } else {
    unsigned long weatherHours = weatherAge / 3600;
    weatherUpdateText = "Pogoda: " + String(weatherHours) + "h temu (co 10min)";
  }
  
  tft.setTextDatum(TL_DATUM);  // ZMIENIONE: wyrównanie do lewej
  tft.drawString(weatherUpdateText, 10, 200);  // ZMIENIONE: nowa pozycja
  
  // 3. Prognoza weekly (trzeci rząd)
  extern WeeklyForecastData weeklyForecast;
  unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
  String weeklyUpdateText;
  if (weeklyAge < 60) {
    weeklyUpdateText = "Pogoda tyg.: " + String(weeklyAge) + "s temu (co 4h)";
  } else if (weeklyAge < 3600) {
    unsigned long weeklyMinutes = weeklyAge / 60;
    weeklyUpdateText = "Pogoda tyg.: " + String(weeklyMinutes) + "min temu (co 4h)";
  } else {
    unsigned long weeklyHours = weeklyAge / 3600;
    weeklyUpdateText = "Pogoda tyg.: " + String(weeklyHours) + "h temu (co 4h)";
  }
  
  tft.setTextDatum(TL_DATUM);  // ZMIENIONE: wyrównanie do lewej
  tft.drawString(weeklyUpdateText, 10, 215);  // ZMIENIONE: trzeci rząd
  
  // 4. Stan WiFi (czwarty rząd) - NOWE!
  String wifiStatus;
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    wifiStatus = "WiFi: " + String(WiFi.SSID()) + " (" + String(rssi) + "dBm)";
  } else {
    wifiStatus = "WiFi: Rozlaczony";
  }
  
  // Skróć jeśli za długi
  if (wifiStatus.length() > 35) {
    wifiStatus = wifiStatus.substring(0, 32) + "...";
  }
  
  tft.drawString(wifiStatus, 10, 230);  // NOWE: czwarty rząd
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  // Ekran 5: Zdjęcie z GitHub
  displayGitHubImage(tft);
}

void ScreenManager::resetWeatherAndTimeCache() {
  // Coordination z Phase 1+2 managers - teraz includes są w .cpp
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}