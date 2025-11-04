#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// --- KONFIGURACJA ---
#include "config/wifi_config.h"
#include "config/weather_config.h"
#include "config/display_config.h"

// --- DANE I API ---
#include "weather/weather_data.h"
#include "weather/weather_api.h"
#include "weather/forecast_data.h"
#include "weather/forecast_api.h"

// --- WYŚWIETLANIE ---
#include "display/weather_display.h"
#include "display/forecast_display.h"
#include "display/time_display.h"
#include "display/screen_manager.h"

// --- TESTY ---
#include "test/weather_test.h"


// --- GLOBALNE OBIEKTY ---
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32 Weather Station - TEST MODE ===");

  // --- Inicjalizacja TFT ---
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(FONT_SIZE_LARGE);

  tft.drawString("TEST MODE", tft.width() / 2, tft.height() / 2 - 20);
  tft.drawString("Laczenie WiFi...", tft.width() / 2, tft.height() / 2 + 20);
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  // --- Łączenie z WiFi (opcjonalne w trybie testowym) ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) { // Tylko 10 prób
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    // --- Konfiguracja czasu ---
    Serial.println("Configuring time...");
    configTzTime(TIMEZONE_INFO, NTP_SERVER);
  } else {
    Serial.println("\nWiFi failed - continuing in offline test mode");
  }
  
  tft.fillScreen(COLOR_BACKGROUND); // Wyczyść ekran
  
  // Pierwsze pobranie pogody i prognozy
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Getting initial weather...");
    getWeather();
    Serial.println("Getting initial forecast...");
    getForecast();
  }
  
  Serial.println("=== LIVE WEATHER MODE READY ===");
  Serial.println("Commands:");
  Serial.println("  'f' - force forecast update");
  Serial.println("  'w' - force weather update");
  Serial.println("=======================");
}

void loop() {
  // --- OBSŁUGA KOMEND SERIAL ---
  if (Serial.available()) {
    char command = Serial.read();
    switch (command) {
      case 'f':
      case 'F':
        // Wymuś pobranie prognozy
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Force forecast update...");
          getForecast();
        }
        break;
      case 'w':
      case 'W':
        // Wymuś pobranie aktualnej pogody
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Force weather update...");
          getWeather();
        }
        break;
    }
  }

  // --- ZARZĄDZANIE EKRANAMI ---
  updateScreenManager();

  // --- PRAWDZIWA AKTUALIZACJA POGODY (co 10 minut) ---
  static unsigned long lastWeatherCheck = 0;
  if (millis() - lastWeatherCheck >= 600000) { // 10 minut
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Updating weather...");
      getWeather();
    }
    lastWeatherCheck = millis();
  }

  // --- AKTUALIZACJA PROGNOZY (co 30 minut) ---
  static unsigned long lastForecastCheck = 0;
  if (millis() - lastForecastCheck >= 1800000) { // 30 minut
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Updating forecast...");
      getForecast();
    }
    lastForecastCheck = millis();
  }

  // --- WYŚWIETLANIE ODPOWIEDNIEGO EKRANU ---
  static ScreenType previousScreen = SCREEN_CURRENT_WEATHER;
  static unsigned long lastDisplayUpdate = 0;
  
  // Sprawdź czy ekran się zmienił - wtedy wymuś pełne odświeżenie
  if (currentScreen != previousScreen) {
    Serial.println("=== DEBUG: ZMIANA EKRANU ===");
    Serial.println("Poprzedni ekran: " + String(previousScreen));
    Serial.println("Nowy ekran: " + String(currentScreen));
    Serial.println("weather.isValid przed zmianą: " + String(weather.isValid ? "TRUE" : "FALSE"));
    
    switchToNextScreen(tft);
    
    Serial.println("weather.isValid po zmianie: " + String(weather.isValid ? "TRUE" : "FALSE"));
    Serial.println("=== DEBUG: KONIEC ZMIANY EKRANU ===");
    
    previousScreen = currentScreen;
    lastDisplayUpdate = millis();
  }
  // Odświeżaj tylko ekran aktualnej pogody (z debugiem)
  else if (currentScreen == SCREEN_CURRENT_WEATHER && millis() - lastDisplayUpdate > 1000) {
    Serial.println("=== DEBUG: Odświeżanie ekranu CURRENT_WEATHER ===");
    Serial.println("WiFi status: " + String(WiFi.status()));
    Serial.println("weather.isValid: " + String(weather.isValid ? "TRUE" : "FALSE"));
    Serial.println("weather.temperature: " + String(weather.temperature));
    Serial.println("weather.description: " + String(weather.description));
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("DEBUG: Wyświetlam czas...");
      displayTime(tft);
    }
    
    if (weather.isValid) {
      Serial.println("DEBUG: Wyświetlam pogodę - weather.isValid=TRUE");
      displayWeather(tft);
      Serial.println("DEBUG: displayWeather() wywołane");
    } else {
      Serial.println("DEBUG: weather.isValid=FALSE - pokazuję LOADING");
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("LOADING WEATHER...", tft.width() / 2, 50);
    }
    
    lastDisplayUpdate = millis();
    Serial.println("=== DEBUG: Koniec odświeżania ===");
  }

  delay(100); // Krótka pauza
}