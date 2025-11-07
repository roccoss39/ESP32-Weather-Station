#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>

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
#include "display/github_image.h"

// --- SENSORY ---
#include "sensors/motion_sensor.h"

// Testy zostały usunięte


// --- GLOBALNE OBIEKTY ---
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1000); // Stabilizacja po wake up
  
  // Sprawdź przyczynę restartu/wake up
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  Serial.println("=== ESP32 Weather Station ===");
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("🔥 WAKE UP: PIR Motion Detected!");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("⏰ WAKE UP: Timer");
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      Serial.println("🚀 COLD START: Power On/Reset");
      break;
  }

  // --- Inicjalizacja TFT ---
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(FONT_SIZE_LARGE);

  tft.drawString("WEATHER STATION", tft.width() / 2, tft.height() / 2 - 20);
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
    Serial.println("\nWiFi failed - funkcje API niedostępne");
    
    // Wyświetl błąd połączenia WiFi
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BLAD WiFi", tft.width() / 2, tft.height() / 2 - 20);
    tft.setTextSize(1);
    tft.drawString("Sprawdz ustawienia sieci", tft.width() / 2, tft.height() / 2 + 10);
    delay(3000);
  }
  
  tft.fillScreen(COLOR_BACKGROUND); // Wyczyść ekran
  
  // --- Inicjalizacja czujnika ruchu PIR ---
  initMotionSensor();
  
  // Display już jest aktywny po initMotionSensor() - nie potrzeba podwójnej aktywacji
  
  // Inicjalizacja systemu NASA images
  initNASAImageSystem();
  
  // Pierwsze pobranie pogody i prognozy z obsługą błędów
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Pobieranie danych pogodowych...");
    
    // Wyświetl status ładowania
    tft.setTextColor(TFT_YELLOW, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.drawString("LADOWANIE", tft.width() / 2, tft.height() / 2 - 20);
    tft.setTextSize(1);
    tft.drawString("Pobieranie pogody...", tft.width() / 2, tft.height() / 2 + 10);
    
    getWeather();
    if (!weather.isValid) {
      Serial.println("BLAD: Nie udalo sie pobrac danych pogodowych");
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.drawString("BLAD API POGODY", tft.width() / 2, tft.height() / 2 + 30);
      delay(2000);
    }
    
    tft.drawString("Pobieranie prognozy...", tft.width() / 2, tft.height() / 2 + 10);
    getForecast();
    if (!forecast.isValid) {
      Serial.println("BLAD: Nie udalo sie pobrac prognozy");
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.drawString("BLAD API PROGNOZY", tft.width() / 2, tft.height() / 2 + 50);
      delay(2000);
    }
    
    if (weather.isValid && forecast.isValid) {
      // Usuń napis "GOTOWE" - przejdź od razu do ekranów
      Serial.println("Dane załadowane - uruchamiam ekrany");
    }
  }
  
  Serial.println("=== STACJA POGODOWA GOTOWA ===");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Komendy Serial:");
    Serial.println("  'f' - wymusz aktualizacje prognozy");
    Serial.println("  'w' - wymusz aktualizacje pogody");
  } else {
    Serial.println("Tryb offline - brak polaczenia WiFi");
  }
  Serial.println("=======================");
}

void loop() {
  // --- OBSŁUGA CZUJNIKA RUCHU PIR ---
  updateDisplayPowerState(tft);
  
  // Jeśli display śpi, nie wykonuj reszty operacji
  if (getDisplayState() == DISPLAY_SLEEPING) {
    delay(100);
    return;
  }
  
  // --- OBSŁUGA KOMEND SERIAL ---
  if (Serial.available()) {
    char command = Serial.read();
    switch (command) {
      case 'f':
      case 'F':
        // Wymuś pobranie prognozy
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Wymuszam aktualizacje prognozy...");
          getForecast();
          if (forecast.isValid) {
            Serial.println("✓ Prognoza zaktualizowana");
          } else {
            Serial.println("✗ Blad aktualizacji prognozy");
          }
        } else {
          Serial.println("✗ Brak połączenia WiFi");
        }
        break;
      case 'w':
      case 'W':
        // Wymuś pobranie aktualnej pogody
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Wymuszam aktualizacje pogody...");
          getWeather();
          if (weather.isValid) {
            Serial.println("✓ Pogoda zaktualizowana");
          } else {
            Serial.println("✗ Blad aktualizacji pogody");
          }
        } else {
          Serial.println("✗ Brak połączenia WiFi");
        }
        break;
    }
  }

  // --- ZARZĄDZANIE EKRANAMI (tylko gdy display aktywny) ---
  updateScreenManager();

  // --- AUTOMATYCZNA AKTUALIZACJA POGODY (co 10 minut) ---
  static unsigned long lastWeatherCheck = 0;
  if (millis() - lastWeatherCheck >= 600000) { // 10 minut
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Automatyczna aktualizacja pogody...");
      getWeather();
      if (!weather.isValid) {
        Serial.println("⚠️ Blad automatycznej aktualizacji pogody");
      }
    }
    lastWeatherCheck = millis();
  }

  // --- AUTOMATYCZNA AKTUALIZACJA PROGNOZY (co 30 minut) ---
  static unsigned long lastForecastCheck = 0;
  if (millis() - lastForecastCheck >= 1800000) { // 30 minut
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Automatyczna aktualizacja prognozy...");
      getForecast();
      if (!forecast.isValid) {
        Serial.println("⚠️ Blad automatycznej aktualizacji prognozy");
      }
    }
    lastForecastCheck = millis();
  }

  // --- WYŚWIETLANIE ODPOWIEDNIEGO EKRANU ---
  static ScreenType previousScreen = SCREEN_CURRENT_WEATHER;
  static unsigned long lastDisplayUpdate = 0;
  
  // Sprawdź czy ekran się zmienił - wtedy wymuś pełne odświeżenie
  if (currentScreen != previousScreen) {
    switchToNextScreen(tft);
    previousScreen = currentScreen;
    lastDisplayUpdate = millis();
  }
  // Odświeżaj ekran aktualnej pogody (co sekundę)
  else if (currentScreen == SCREEN_CURRENT_WEATHER && millis() - lastDisplayUpdate > 1000) {
    // Aktualizuj czas (jeśli WiFi działa)
    if (WiFi.status() == WL_CONNECTED) {
      displayTime(tft);
    }
    
    // Aktualizuj pogodę lub pokaż błąd
    if (weather.isValid) {
      displayWeather(tft);
    } else {
      // Pokaż komunikat o braku danych
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("BRAK DANYCH", tft.width() / 2, 50);
      tft.setTextSize(1);
      if (WiFi.status() != WL_CONNECTED) {
        tft.drawString("Sprawdz polaczenie WiFi", tft.width() / 2, 80);
      } else {
        tft.drawString("Blad API pogody", tft.width() / 2, 80);
      }
    }
    
    lastDisplayUpdate = millis();
  }

  delay(100); // Krótka pauza
}