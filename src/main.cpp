#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include <Preferences.h>

// --- KONFIGURACJA ---
#include "config/wifi_config.h"
#include "config/weather_config.h"
#include "config/display_config.h"
#include "config/secrets.h"

// --- DANE I API ---
#include "weather/weather_data.h"
#include "weather/weather_api.h"
#include "weather/forecast_data.h"
#include "weather/forecast_api.h"

// --- WYŚWIETLANIE ---
#include "display/weather_display.h"
#include "display/forecast_display.h"
#include "display/time_display.h"
#include "managers/ScreenManager.h"
#include "display/screen_manager.h"
#include "display/github_image.h"

// --- SENSORY ---
#include "sensors/motion_sensor.h"

// --- WIFI TOUCH INTERFACE ---
#include "wifi/wifi_touch_interface.h"

// --- EXPLICIT FUNCTION DECLARATIONS (fix for compilation) ---
extern void updateScreenManager();
extern void switchToNextScreen(TFT_eSPI& tft);
extern ScreenManager& getScreenManager();

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
  
  // --- Kalibracja dotyku z test_wifi ---
  uint16_t calData[5] = { 350, 3267, 523, 3020, 1 };
  tft.setTouch(calData);
  Serial.println("Touch calibration applied!");
  
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(FONT_SIZE_LARGE);

  tft.drawString("WEATHER STATION", tft.width() / 2, tft.height() / 2 - 20);
  tft.drawString("Laczenie WiFi...", tft.width() / 2, tft.height() / 2 + 20);
  
  // --- AUTO-CONNECT: Spróbuj najpierw zapisanych danych z WiFi Touch Interface ---
  String savedSSID = "";
  String savedPassword = "";
  
  // Sprawdź czy są zapisane dane WiFi
  Preferences prefs;
  prefs.begin("wifi", true); // readonly
  savedSSID = prefs.getString("ssid", "");
  savedPassword = prefs.getString("password", "");
  prefs.end();
  
  String connectSSID = WIFI_SSID;
  String connectPassword = WIFI_PASSWORD;
  
  // Jeśli są zapisane dane, użyj ich zamiast domyślnych
  if (savedSSID.length() > 0 && savedPassword.length() > 0) {
    connectSSID = savedSSID;
    connectPassword = savedPassword;
    Serial.print("AUTO-CONNECT to saved WiFi: ");
    Serial.println(connectSSID);
  } else {
    Serial.print("Using default WiFi from secrets.h: ");
    Serial.println(connectSSID);
  }

  WiFi.begin(connectSSID.c_str(), connectPassword.c_str());
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
  
  // --- Inicjalizacja WiFi Touch Interface ---
  initWiFiTouchInterface();
  
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
  // --- OBSŁUGA WIFI TOUCH INTERFACE ---
  // Sprawdź czy WiFi config jest aktywny (ma priorytet nad wszystkim)
  if (isWiFiConfigActive()) {
    handleWiFiTouchLoop(tft);
    return; // Skip normal operation during WiFi config
  }
  
  // --- AUTO-RECONNECT SYSTEM (z test_wifi) ---
  // Wywołaj system auto-reconnect nawet gdy WiFi config nie jest aktywny
  static unsigned long lastWiFiSystemCheck = 0;
  if (millis() - lastWiFiSystemCheck > 2000) { // Co 2 sekundy jak w test_wifi
    lastWiFiSystemCheck = millis();
    
    // Wywołaj funkcje z wifi_touch_interface.cpp które obsługują auto-reconnect
    extern void checkWiFiConnection();
    extern void handleWiFiLoss();
    extern void handleBackgroundReconnect();
    
    checkWiFiConnection();
    handleWiFiLoss();
    handleBackgroundReconnect();
  }
  
  // --- OBSŁUGA CZUJNIKA RUCHU PIR ---
  updateDisplayPowerState(tft);
  
  // Jeśli display śpi, nie wykonuj reszty operacji
  if (getDisplayState() == DISPLAY_SLEEPING) {
    delay(50); // Krótka pauza dla PIR check
    return;
  }
  
  // --- SPRAWDŹ TRIGGERY WIFI CONFIG ---
  // Trigger 1: Long press 5 sekund
  if (checkWiFiLongPress(tft)) {
    Serial.println("🌐 LONG PRESS DETECTED - Entering WiFi config!");
    enterWiFiConfigMode(tft);
    return;
  }
  
  // Trigger 2: WiFi connection lost - teraz obsługiwane przez handleWiFiLoss() 
  // (usuń duplikujące sprawdzenie - auto-reconnect system jest lepszy)
  
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

  // --- ZARZĄDZANIE EKRANAMI (tylko gdy display aktywny i nie ma WiFi config) ---
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
  ScreenType currentScreen = getScreenManager().getCurrentScreen();
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

  delay(50); // Optymalizowana pauza
}