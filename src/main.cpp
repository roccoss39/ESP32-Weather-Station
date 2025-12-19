#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include "managers/SystemManager.h"
#include "config/hardware_config.h"

// === FLAGA BLOKADY WiFi PODCZAS POBIERANIA OBRAZKA ===
bool isImageDownloadInProgress = false;

// --- KONFIGURACJA ---
#include "config/display_config.h"
#include "config/secrets.h"
#include "config/timing_config.h"
#include "config/location_config.h"

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
#include "sensors/dht22_sensor.h" // <--- DODANO: Obsługa DHT22

// --- WIFI TOUCH INTERFACE ---
#include "wifi/wifi_touch_interface.h"

// --- EXPLICIT FUNCTION DECLARATIONS (fix for compilation) ---
extern void updateScreenManager();
extern void switchToNextScreen(TFT_eSPI& tft);
extern ScreenManager& getScreenManager();
void onWiFiConnectedTasks();

// --- GLOBALNE OBIEKTY ---
TFT_eSPI tft = TFT_eSPI();
SystemManager sysManager;

// --- GLOBALNE FLAGI ERROR MODE ---
bool weatherErrorModeGlobal = false;
bool forecastErrorModeGlobal = false;
bool weeklyErrorModeGlobal = false;
bool isNtpSyncPending = false;      
bool isLocationSavePending = false; 

// --- GLOBALNE TIMERY (żeby można je resetować z setup) ---
unsigned long lastWeatherCheckGlobal = 0;
unsigned long lastForecastCheckGlobal = 0;


void setup() {
  Serial.begin(115200);
  delay(DELAY_STABILIZATION); // Stabilizacja po wake up
  
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
  
  sysManager.init();

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
  
  // FIXED: Zapisane dane NADPISUJĄ defaults zawsze gdy są dostępne  
  if (savedSSID.length() > 0) {  // Wystarczy tylko SSID, hasło może być puste dla open networks
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
    Serial.println("Configuring time from NTP server...");
    configTzTime(TIMEZONE_INFO, NTP_SERVER);

    // --- POPRAWKA: POCZEKAJ NA SYNCHRONIZACJĘ CZASU ---
    // Wywołania API (HTTPS) nie powiodą się, jeśli czas nie jest ustawiony.
    Serial.print("Waiting for time synchronization...");

    struct tm timeinfo;
    int retry = 0;
    const int retry_count = 15; // 15 sekund timeout

    // Sprawdź, czy czas jest poprawny (rok > 2023)
    while (!getLocalTime(&timeinfo, 5000) || timeinfo.tm_year < (2023 - 1900)) {
        Serial.print(".");
        delay(1000);
        retry++;
        if (retry > retry_count) {
            Serial.println("\nFailed to synchronize time!");
            break; 
        }
    }

    if (retry <= retry_count) {
      Serial.println("\nTime synchronized successfully!");
    }
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

  // --- Inicjalizacja czujnika DHT22 ---
  initDHT22(); // <--- DODANO: Inicjalizacja DHT
  
  // --- Inicjalizacja lokalizacji ---
  locationManager.loadLocationFromPreferences();
  
  // --- Inicjalizacja WiFi Touch Interface ---
  initWiFiTouchInterface();
  
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
      Serial.println("BLAD: Nie udalo sie pobrac danych pogodowych - AKTYWUJĘ ERROR MODE");
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.drawString("BLAD API POGODY", tft.width() / 2, tft.height() / 2 + 30);
      delay(2000);
      
      weatherErrorModeGlobal = true;
      lastWeatherCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
      Serial.println("Weather error mode AKTYWNY - natychmiastowy retry potem co 20s");
    }
    
    tft.drawString("Pobieranie prognozy...", tft.width() / 2, tft.height() / 2 + 10);
    getForecast();
    
    // Wygeneruj takze prognoza 5-dniowa przy starcie
    generateWeeklyForecast();
    
    if (!forecast.isValid) {
      Serial.println("BLAD: Nie udalo sie pobrac prognozy - AKTYWUJĘ ERROR MODE");
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.drawString("BLAD API PROGNOZY", tft.width() / 2, tft.height() / 2 + 50);
      delay(2000);
      
      forecastErrorModeGlobal = true;
      lastForecastCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
      
      Serial.println("Forecast error mode AKTYWNY - pierwszy retry za 20s");
    }
    
    if (weather.isValid && forecast.isValid) {
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

  sysManager.loop();
  // --- OBSŁUGA CZUJNIKA RUCHU PIR (NAJWYŻSZY PRIORYTET) ---
  updateDisplayPowerState(tft, isWiFiConfigActive());

  // --- AKTUALIZACJA DHT22 (NIEBLOKUJĄCA) ---
  updateDHT22(); // <--- DODANO: Odczyt czujnika w pętli


  // Jeśli display śpi, nie wykonuj reszty operacji
  if (getDisplayState() == DISPLAY_SLEEPING) {
    delay(50); 
    return;
  }

  // --- OBSŁUGA WIFI TOUCH INTERFACE ---
  if (isWiFiConfigActive()) {
    handleWiFiTouchLoop(tft);
    return; 
  }

  if (isNtpSyncPending) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10) && timeinfo.tm_year > (2023 - 1900)) {
        Serial.println("\nTime synchronized successfully! (from loop)");
        isNtpSyncPending = false; 
    } else {
        Serial.print("t"); 
        delay(500); 
        return; 
    }
  }

  if (isLocationSavePending) {
    Serial.println("LOOP: Wykryto flagę zapisu lokalizacji. Zapisywanie do Preferences...");
    locationManager.saveLocationToPreferences();
    isLocationSavePending = false; 
    Serial.println("LOOP: Zapis lokalizacji zakończony.");
  }
  
  // --- AUTO-RECONNECT SYSTEM ---
  static unsigned long lastWiFiSystemCheck = 0;
  if (millis() - lastWiFiSystemCheck > WIFI_STATUS_CHECK_INTERVAL) { 
    lastWiFiSystemCheck = millis();
    
    extern void checkWiFiConnection();
    extern void handleWiFiLoss();
    extern void handleBackgroundReconnect();
    extern bool isWiFiLost();
    
    checkWiFiConnection();
    handleWiFiLoss();
    handleBackgroundReconnect();
    
    if (isWiFiLost()) {
      Serial.println("🔴 WiFi LOST - Screen rotation PAUSED until reconnect");
      return; 
    }
  }
  
  // --- SPRAWDŹ TRIGGERY WIFI CONFIG ---
  if (checkWiFiLongPress(tft)) {
    Serial.println("🌐 LONG PRESS DETECTED - Entering WiFi config!");
    enterWiFiConfigMode(tft);
    return;
  }
  
  // --- OBSŁUGA KOMEND SERIAL ---
  if (Serial.available()) {
    char command = Serial.read();
    switch (command) {
      case 'f':
      case 'F':
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Wymuszam aktualizacje prognozy...");
          getForecast();
          if (forecast.isValid) Serial.println("✓ Prognoza zaktualizowana");
          else Serial.println("✗ Blad aktualizacji prognozy");
        } else Serial.println("✗ Brak połączenia WiFi");
        break;
      case 'w':
      case 'W':
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Wymuszam aktualizacje pogody...");
          getWeather();
          if (weather.isValid) Serial.println("✓ Pogoda zaktualizowana");
          else Serial.println("✗ Blad aktualizacji pogody");
        } else Serial.println("✗ Brak połączenia WiFi");
        break;
      case 'x':
      case 'X':
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Wymuszam aktualizacje weekly forecast...");
          if (generateWeeklyForecast()) Serial.println("✓ Weekly forecast zaktualizowany");
          else Serial.println("✗ Blad aktualizacji weekly forecast");
        } else Serial.println("✗ Brak połączenia WiFi");
        break;
      default:
        Serial.println("Dostepne komendy: 'f', 'w', 'x'");
        break;
    }
  }

  // --- ZARZĄDZANIE EKRANAMI ---
  extern bool isWiFiLost();
  if (!isWiFiLost()) {
    updateScreenManager();
  } else {
    Serial.println("🔴 WiFi LOST - Screen manager PAUSED");
  }

  // === WEEKLY FORECAST ===
  unsigned long weeklyInterval = weeklyErrorModeGlobal ? WEEKLY_UPDATE_ERROR : WEEKLY_UPDATE_INTERVAL;
  if (millis() - lastWeeklyUpdate >= weeklyInterval) {
    lastWeeklyUpdate = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (weeklyErrorModeGlobal) Serial.println("Retry weekly forecast po błędzie (5s)...");
      else Serial.println("Automatyczna aktualizacja weekly forecast (4h)...");
      
      if (generateWeeklyForecast()) {
        if (weeklyErrorModeGlobal) {
          Serial.println("✓ Weekly forecast naprawiony");
          weeklyErrorModeGlobal = false;
        }
      } else {
        Serial.println("❌ Weekly forecast update failed - aktywuję error mode");
        weeklyErrorModeGlobal = true;
      }
    }
  }

  // --- AUTOMATYCZNA AKTUALIZACJA POGODY ---
  unsigned long weatherInterval;
  if (weatherErrorModeGlobal) weatherInterval = WEATHER_UPDATE_ERROR;   
  else weatherInterval = WEATHER_UPDATE_NORMAL;  
  
  if (millis() - lastWeatherCheckGlobal >= weatherInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      if (weatherErrorModeGlobal) Serial.println("Retry pogody po błędzie (20s)...");
      else Serial.println("Automatyczna aktualizacja pogody (10 min)...");
      
      getWeather();
      
      if (weather.isValid) {
        if (weatherErrorModeGlobal) {
          Serial.println("✓ Pogoda naprawiona");
          weatherErrorModeGlobal = false;
          if (getScreenManager().getCurrentScreen() == SCREEN_CURRENT_WEATHER) {
            switchToNextScreen(tft);
          }
        }
      } else {
        Serial.println("⚠️ Błąd pogody - przełączam na 20s retry");
        weatherErrorModeGlobal = true;
      }
    }
    lastWeatherCheckGlobal = millis();
  }

  // --- AUTOMATYCZNA AKTUALIZACJA PROGNOZY ---
  unsigned long forecastInterval;
  if (forecastErrorModeGlobal) forecastInterval = WEATHER_UPDATE_ERROR;   
  else forecastInterval = 1800000; 
  
  if (millis() - lastForecastCheckGlobal >= forecastInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      if (forecastErrorModeGlobal) Serial.println("Retry prognozy po błędzie (20s)...");
      else Serial.println("Automatyczna aktualizacja prognozy (30 min)...");
      
      getForecast();
      
      if (forecast.isValid) {
        if (forecastErrorModeGlobal) {
          Serial.println("✓ Prognoza naprawiona");
          forecastErrorModeGlobal = false;
          if (getScreenManager().getCurrentScreen() == SCREEN_FORECAST) {
            switchToNextScreen(tft);
          }
        }
      } else {
        Serial.println("⚠️ Błąd prognozy - przełączam na 20s retry");
        forecastErrorModeGlobal = true;
      }
    }
    lastForecastCheckGlobal = millis();
  }

  // --- ODŚWIEŻANIE WYŚWIETLACZA W LOOP ---
  static ScreenType previousScreen = SCREEN_CURRENT_WEATHER;
  static unsigned long lastDisplayUpdate = 0;
  
  if (!isWiFiLost()) {
    ScreenType currentScreen = getScreenManager().getCurrentScreen();
    if (currentScreen != previousScreen) {
      switchToNextScreen(tft);
      previousScreen = currentScreen;
      lastDisplayUpdate = millis();
    }
    else if (currentScreen == SCREEN_CURRENT_WEATHER && millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
      if (WiFi.status() == WL_CONNECTED) {
        displayTime(tft);
      }
      if (weather.isValid) {
        displayWeather(tft);
      } else {
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("BRAK DANYCH", tft.width() / 2, 50);
        tft.setTextSize(1);
        if (WiFi.status() != WL_CONNECTED) tft.drawString("Sprawdz polaczenie WiFi", tft.width() / 2, 80);
        else tft.drawString("Blad API pogody", tft.width() / 2, 80);
      }
      lastDisplayUpdate = millis();
    }
  } 

  delay(50); 
}

void onWiFiConnectedTasks() {
    Serial.println("onWiFiConnectedTasks: WiFi connected. Triggering NON-BLOCKING NTP sync...");
    configTzTime(TIMEZONE_INFO, NTP_SERVER);
    isNtpSyncPending = true; 
    
    Serial.println("Forcing immediate API fetch (pending NTP sync)...");
    weatherErrorModeGlobal = true;
    forecastErrorModeGlobal = true;
    weeklyErrorModeGlobal = true;
    lastWeatherCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    lastForecastCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    
    extern unsigned long lastWeeklyUpdate;
    lastWeeklyUpdate = millis() - WEEKLY_UPDATE_INTERVAL; 
    Serial.println("Forcing immediate weekly forecast update after WiFi reconnect");
}