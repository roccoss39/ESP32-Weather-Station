#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include "managers/SystemManager.h"
#include "config/hardware_config.h"
#include "managers/GithubUpdateManager.h"

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
#include "sensors/dht22_sensor.h" 

// --- WIFI TOUCH INTERFACE ---
#include "wifi/wifi_touch_interface.h"

// --- EXTERNAL FUNCTION DECLARATIONS ---
// Deklaracje funkcji z innych modułów, aby loop() je widział
extern void updateScreenManager();
extern void switchToNextScreen(TFT_eSPI& tft);
extern ScreenManager& getScreenManager();
extern void checkWiFiConnection();
extern void handleWiFiLoss();
extern void handleBackgroundReconnect();
extern bool isWiFiLost();
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

// --- GLOBALNE TIMERY ---
unsigned long lastWeatherCheckGlobal = 0;
unsigned long lastForecastCheckGlobal = 0;
unsigned long lastWeeklyUpdate = 0; // <--- DODANO BRAKUJĄCĄ ZMIENNĄ

void setup() {
  Serial.begin(115200);
  delay(DELAY_STABILIZATION); 
  
  // 1. Inicjalizuj SystemManagera (kontrola zasilania/PWM)
  sysManager.init(); 
  // WAŻNE: Ekran wygaszony na start (stealth mode dla nocnych update'ów)
  sysManager.setBrightness(0);

  // Sprawdź przyczynę restartu/wake up
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  Serial.println("=== ESP32 Weather Station ===");
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("🔥 WAKE UP: PIR Motion Detected!");
      break;

    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("⏰ WAKE UP: NOCNA AKTUALIZACJA (03:00)");
      
      // === LOGIKA "PO CICHU" ===
      // Nie inicjalizujemy TFT, żeby ekran nie błysnął.
      {
          Preferences prefs;
          prefs.begin("wifi", true);
          String ssid = prefs.getString("ssid", WIFI_SSID); 
          String pass = prefs.getString("password", WIFI_PASSWORD);
          prefs.end();

          Serial.print("Connecting to WiFi for update: ");
          Serial.println(ssid);
          
          WiFi.begin(ssid.c_str(), pass.c_str());
          
          int retries = 0;
          while (WiFi.status() != WL_CONNECTED && retries < 20) {
              delay(500);
              Serial.print(".");
              retries++;
              sysManager.loop(); // Karm psa!
          }
          
          if (WiFi.status() == WL_CONNECTED) {
              Serial.println("\n✅ WiFi Connected. Checking GitHub...");
              
              GithubUpdateManager updateMgr;
              updateMgr.checkForUpdate(); 
              // Jeśli znajdzie update -> zrestartuje się sam.
          } else {
              Serial.println("\n❌ WiFi Failed. Update skipped.");
          }
      }
      
      Serial.println("💤 Wracam spać do rana...");
      Serial.flush();

      // Idź spać (tylko PIR aktywny)
      esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1); 
      esp_deep_sleep_start(); // STOP! Procesor idzie spać tutaj.
      break;

    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      Serial.println("🚀 COLD START: Power On/Reset");
      break;
  }

  // --- Inicjalizacja TFT ---
  tft.init();
  tft.setRotation(1);
  
  // --- Kalibracja dotyku ---
  uint16_t calData[5] = { 350, 3267, 523, 3020, 1 };
  tft.setTouch(calData);
  Serial.println("Touch calibration applied!");
  
  tft.fillScreen(COLOR_BACKGROUND);
  
  // Włączamy podświetlenie (jasność startowa)
  sysManager.restoreCorrectBrightness();

  tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(FONT_SIZE_LARGE);

  tft.drawString("WEATHER STATION", tft.width() / 2, tft.height() / 2 - 20);
  tft.drawString("Laczenie WiFi...", tft.width() / 2, tft.height() / 2 + 20);
  
  // --- AUTO-CONNECT ---
  String savedSSID = "";
  String savedPassword = "";
  
  Preferences prefs;
  prefs.begin("wifi", true); 
  savedSSID = prefs.getString("ssid", "");
  savedPassword = prefs.getString("password", "");
  prefs.end();
  
  String connectSSID = WIFI_SSID;
  String connectPassword = WIFI_PASSWORD;
  
  if (savedSSID.length() > 0) {  
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
  while (WiFi.status() != WL_CONNECTED && attempts < 10) { 
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

    Serial.print("Waiting for time synchronization...");
    struct tm timeinfo;
    int retry = 0;
    const int retry_count = 15; 

    // Blokujące oczekiwanie na czas (wymagane dla HTTPS)
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
    
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("BLAD WiFi", tft.width() / 2, tft.height() / 2 - 20);
    tft.setTextSize(1);
    tft.drawString("Sprawdz ustawienia sieci", tft.width() / 2, tft.height() / 2 + 10);
    delay(3000);
  }
  
  tft.fillScreen(COLOR_BACKGROUND); 
  
  // --- Inicjalizacja sensorów i modułów ---
  initMotionSensor();
  initDHT22();
  locationManager.loadLocationFromPreferences();
  initWiFiTouchInterface();
  initNASAImageSystem();
  
  // --- Pierwsze pobranie danych ---
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Pobieranie danych pogodowych...");
    
    tft.setTextColor(TFT_YELLOW, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.drawString("LADOWANIE", tft.width() / 2, tft.height() / 2 - 20);
    tft.setTextSize(1);
    tft.drawString("Pobieranie pogody...", tft.width() / 2, tft.height() / 2 + 10);
    
    getWeather();
    if (!weather.isValid) {
      weatherErrorModeGlobal = true;
      lastWeatherCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
      Serial.println("Weather error mode AKTYWNY");
    }
    
    tft.drawString("Pobieranie prognozy...", tft.width() / 2, tft.height() / 2 + 10);
    getForecast();
    generateWeeklyForecast();
    
    if (!forecast.isValid) {
      forecastErrorModeGlobal = true;
      lastForecastCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
      Serial.println("Forecast error mode AKTYWNY");
    }
    
    if (weather.isValid && forecast.isValid) {
      Serial.println("Dane załadowane - uruchamiam ekrany");
    }
  }
  
  Serial.println("=== STACJA POGODOWA GOTOWA ===");
}

void loop() {
  sysManager.loop(); // Watchdog i zadania systemowe

  // --- OBSŁUGA CZUJNIKA RUCHU PIR (NAJWYŻSZY PRIORYTET) ---
  updateDisplayPowerState(tft, isWiFiConfigActive());

  // --- AKTUALIZACJA DHT22 ---
  updateDHT22();

  // Jeśli display śpi, nie wykonuj reszty (oszczędzanie CPU)
  if (getDisplayState() == DISPLAY_SLEEPING) {
    delay(50); 
    return;
  }

  // --- OBSŁUGA WIFI TOUCH INTERFACE ---
  if (isWiFiConfigActive()) {
    handleWiFiTouchLoop(tft);
    return; 
  }

  // --- NTP ASYNC CHECK ---
  if (isNtpSyncPending) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10) && timeinfo.tm_year > (2023 - 1900)) {
        Serial.println("\nTime synchronized successfully! (from loop)");
        isNtpSyncPending = false; 
    }
    // UWAGA: Nie robimy tu 'return', żeby nie blokować pętli, jeśli NTP leży
  }

  // --- ZAPIS LOKALIZACJI ---
  if (isLocationSavePending) {
    Serial.println("LOOP: Zapisywanie lokalizacji...");
    locationManager.saveLocationToPreferences();
    isLocationSavePending = false; 
  }
  
  // --- AUTO-RECONNECT SYSTEM ---
  static unsigned long lastWiFiSystemCheck = 0;
  if (millis() - lastWiFiSystemCheck > WIFI_STATUS_CHECK_INTERVAL) { 
    lastWiFiSystemCheck = millis();
    checkWiFiConnection();
    handleWiFiLoss();
    handleBackgroundReconnect();
  }
  
  // --- TRIGGERY WIFI CONFIG (LONG PRESS) ---
  if (checkWiFiLongPress(tft)) {
    Serial.println("🌐 LONG PRESS - Entering WiFi config!");
    enterWiFiConfigMode(tft);
    return;
  }
  
  // --- OBSŁUGA KOMEND SERIAL ---
 // --- OBSŁUGA KOMEND SERIAL ---
  if (Serial.available()) {
    char command = Serial.read();
    
    // Sprawdzamy WiFi raz dla wszystkich komend wymagających Internetu
    if (WiFi.status() == WL_CONNECTED) {
        
        if (command == 'f' || command == 'F') {
            getForecast();
        }
        else if (command == 'w' || command == 'W') {
            getWeather();
        }
        else if (command == 'x' || command == 'X') {
            generateWeeklyForecast();
        }
        else if (command == 'u' || command == 'U') {
            // === TEST AKTUALIZACJI ===
            Serial.println("🧪 TEST: Wymuszam sprawdzenie aktualizacji z GitHuba...");
          
            GithubUpdateManager updateMgr;
            updateMgr.checkForUpdate(); 
          
            // Jeśli update się uda, procesor zresetuje się wewnątrz checkForUpdate()
            // Jeśli dotarliśmy tutaj, to znaczy, że nie było nowej wersji lub wystąpił błąd
            Serial.println("🏁 Koniec testu aktualizacji (brak nowej wersji lub błąd).");
        }
        
    } else {
        // Opcjonalnie: Info, że nie ma sieci
        if (strchr("fwxuFWXU", command)) { // Jeśli wciśnięto jedną z komend sieciowych
            Serial.println("❌ Ignoruję komendę: Brak połączenia WiFi");
        }
    }
  }

  // --- ZARZĄDZANIE EKRANAMI ---
  if (!isWiFiLost()) {
    updateScreenManager();
  }

  // === WEEKLY FORECAST UPDATE ===
  unsigned long weeklyInterval = weeklyErrorModeGlobal ? WEEKLY_UPDATE_ERROR : WEEKLY_UPDATE_INTERVAL;
  if (millis() - lastWeeklyUpdate >= weeklyInterval) {
    lastWeeklyUpdate = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (generateWeeklyForecast()) {
        weeklyErrorModeGlobal = false;
      } else {
        weeklyErrorModeGlobal = true;
      }
    }
  }

  // --- WEATHER UPDATE ---
  unsigned long weatherInterval = weatherErrorModeGlobal ? WEATHER_UPDATE_ERROR : WEATHER_UPDATE_NORMAL;
  if (millis() - lastWeatherCheckGlobal >= weatherInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      getWeather();
      if (weather.isValid) {
        weatherErrorModeGlobal = false;
        // Odśwież ekran jeśli jesteśmy na ekranie pogody
        if (getScreenManager().getCurrentScreen() == SCREEN_CURRENT_WEATHER) {
           // Opcjonalne: wymuszenie przerysowania, 
           // choć pętla wyświetlania niżej i tak to zrobi
        }
      } else {
        weatherErrorModeGlobal = true;
      }
    }
    lastWeatherCheckGlobal = millis();
  }

  // --- FORECAST UPDATE ---
  unsigned long forecastInterval = forecastErrorModeGlobal ? WEATHER_UPDATE_ERROR : 1800000; 
  if (millis() - lastForecastCheckGlobal >= forecastInterval) {
    if (WiFi.status() == WL_CONNECTED) {
      getForecast();
      if (forecast.isValid) {
        forecastErrorModeGlobal = false;
      } else {
        forecastErrorModeGlobal = true;
      }
    }
    lastForecastCheckGlobal = millis();
  }

  // --- ODŚWIEŻANIE ZAWARTOŚCI EKRANU (ZEGAR, DANE) ---
  static ScreenType previousScreen = SCREEN_CURRENT_WEATHER;
  static unsigned long lastDisplayUpdate = 0;
  
  if (!isWiFiLost()) {
    ScreenType currentScreen = getScreenManager().getCurrentScreen();
    
    // Jeśli zmienił się ekran (np. przez updateScreenManager) - przerysuj całość
    if (currentScreen != previousScreen) {
      switchToNextScreen(tft);
      previousScreen = currentScreen;
      lastDisplayUpdate = millis();
    }
    // Jeśli ekran ten sam, odświeżaj zegar/dane co sekundę
    else if (currentScreen == SCREEN_CURRENT_WEATHER && millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
      if (WiFi.status() == WL_CONNECTED) {
        displayTime(tft);
      }
      if (weather.isValid) {
        displayWeather(tft);
      } else {
        // Obsługa braku danych na ekranie głównym
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("BRAK DANYCH", tft.width() / 2, 50);
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
    
    Serial.println("Forcing immediate API fetch...");
    weatherErrorModeGlobal = true;
    forecastErrorModeGlobal = true;
    weeklyErrorModeGlobal = true;
    
    // Reset timerów, aby wymusić update w najbliższym obiegu pętli loop
    lastWeatherCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    lastForecastCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    lastWeeklyUpdate = millis() - WEEKLY_UPDATE_INTERVAL; 
}