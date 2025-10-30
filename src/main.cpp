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
  
  // --- Inicjalizacja testów ---
  initWeatherTest();
  
  Serial.println("=== TEST MODE READY ===");
  Serial.println("Commands:");
  Serial.println("  'r' - reset test cycle");
  Serial.println("  'v' - validate current data");
  Serial.println("  's' - show current test info");
  Serial.println("=======================");
}

void loop() {
  // --- OBSŁUGA KOMEND SERIAL ---
  if (Serial.available()) {
    char command = Serial.read();
    switch (command) {
      case 'r':
      case 'R':
        resetWeatherTest();
        break;
      case 'v':
      case 'V':
        if (validateWeatherData()) {
          Serial.println("✓ Weather data is valid");
        } else {
          Serial.println("✗ Weather data validation failed!");
        }
        break;
      case 's':
      case 'S':
        Serial.println("Current test: " + String(weather.description));
        Serial.println("Screen: " + String(currentScreen == SCREEN_CURRENT_WEATHER ? "WEATHER" : "FORECAST"));
        Serial.println("Valid: " + String(weather.isValid ? "YES" : "NO"));
        break;
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

  // --- TESTY POGODOWE ---
  runWeatherTest();

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
    Serial.println("Zmiana ekranu - czyszczenie i odswiezanie");
    switchToNextScreen(tft);
    previousScreen = currentScreen;
    lastDisplayUpdate = millis();
  }
  // Odświeżaj tylko ekran aktualnej pogody (dla czasu i testów)
  else if (currentScreen == SCREEN_CURRENT_WEATHER && millis() - lastDisplayUpdate > 1000) {
    if (WiFi.status() == WL_CONNECTED) {
      displayTime(tft);
    }
    
    if (validateWeatherData()) {
      displayWeather(tft);
    } else {
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("DATA ERROR!", tft.width() / 2, 50);
    }
    
    lastDisplayUpdate = millis();
  }

  delay(100); // Krótka pauza
}