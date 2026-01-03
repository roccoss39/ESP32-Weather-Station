#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include "managers/SystemManager.h"
#include "managers/GithubUpdateManager.h"


// --- KONFIGURACJA ---
#include "config/secrets.h"
#include "config/hardware_config.h"
#include "config/display_config.h"
#include "config/timing_config.h"
#include "config/location_config.h"

// --- DANE I API ---
#include "weather/weather_data.h"
#include "weather/weather_api.h"
#include "weather/forecast_data.h"
#include "weather/forecast_api.h"

// --- WYŚWIETLANIE ---
#include "display/current_weather_display.h"
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

// === FLAGA BLOKADY WiFi PODCZAS POBIERANIA OBRAZKA ===
bool isImageDownloadInProgress = false;

// === FLAGA TRYBU OFFLINE (BEZ WIFI) ===
bool isOfflineMode = false; 

// --- EXTERNAL FUNCTION DECLARATIONS ---
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
unsigned long lastWeeklyUpdate = 0; 

void setup() {
  Serial.begin(115200);
  delay(DELAY_STABILIZATION); 
  
  sysManager.init(); 
  sysManager.setBrightness(0);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  Serial.println("=== ESP32 Weather Station ===");
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("🔥 WAKE UP: PIR Motion Detected!");
      break;

    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("⏰ WAKE UP: NOCNA AKTUALIZACJA (03:00)");

      // === DODAJ TEN BLOK TUTAJ: JITTER (Losowe opóźnienie) ===
      {
         // Czekamy losowo od 0 do 300 sekund (5 minut)
         // To zapobiega jednoczesnemu atakowaniu serwera GitHub przez wszystkie stacje
         int jitterSeconds = random(0, FIRMWARE_UPDATE_JITTER + 1);
         Serial.printf("🎲 Jitter: Czekam %d sekund przed sprawdzeniem aktualizacji...\n", jitterSeconds);
         
         // Używamy pętli z delay(1000) żeby móc karmić Watchdoga (sysManager.loop)
         for(int i=0; i<jitterSeconds; i++) {
             delay(1000); 
             sysManager.loop(); // Ważne: Watchdog musi być karmiony!
         }
      }
      // ==========================================================
      
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
          // Zwiększony limit prób dla bezpieczeństwa w nocy
          while (WiFi.status() != WL_CONNECTED && retries < 40) {
              delay(500);
              Serial.print(".");
              retries++;
              sysManager.loop(); 
          }
          
          if (WiFi.status() == WL_CONNECTED) {
              Serial.println("\n✅ WiFi Connected. Checking GitHub...");
              GithubUpdateManager updateMgr;
              updateMgr.checkForUpdate(); 
          } else {
              Serial.println("\n❌ WiFi Failed. Update skipped.");
          }
      }
      
      Serial.println("💤 Wracam spać do rana...");
      Serial.flush();
      esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1); 
      esp_deep_sleep_start(); 
      break;

    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      Serial.println("🚀 COLD START: Power On/Reset");
      break;
  }

  tft.init();
  tft.setRotation(1);
  
const uint16_t* cal = getTouchCalibration();
if (cal) {
    tft.setTouch(const_cast<uint16_t*>(cal));
    Serial.println("✅ Touch calibration applied from secrets.h");
}
else {
    Serial.println("⚠️ No touch calibration found");
}

  tft.fillScreen(COLOR_BACKGROUND);

  // === NOWOŚĆ: EKRAN POWITALNY "DOBREGO DNIA" ===
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 10)) { 
          if (timeinfo.tm_hour >= 5 && timeinfo.tm_hour < 11) {
              Serial.println("☀️ Poranne wybudzenie - wyświetlam powitanie!");
              tft.setTextColor(TFT_ORANGE, COLOR_BACKGROUND); 
              tft.setTextDatum(MC_DATUM); 
              tft.setTextSize(2);
              tft.drawString("Dobrego", tft.width() / 2, tft.height() / 2 - 25);
              tft.setTextSize(3);
              tft.drawString("dnia!", tft.width() / 2, tft.height() / 2 + 15);
              delay(3000);
              tft.fillScreen(COLOR_BACKGROUND);
          }
      }
  }
  
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

    Serial.println("Configuring time from NTP server...");
    configTzTime(TIMEZONE_INFO, NTP_SERVER);

    Serial.print("Waiting for time synchronization...");
    struct tm timeinfo;
    int retry = 0;
    const int retry_count = 15; 

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
  
  // === RESET TIMERA EKRANU ===
  getScreenManager().resetScreenTimer();
  Serial.println("📱 Timer ekranu zresetowany - 10s do następnego przełączenia");
  
  Serial.println("=== STACJA POGODOWA GOTOWA ===");
}


void loop() {
  sysManager.loop(); // Watchdog i zadania systemowe

  // --- OBSŁUGA CZUJNIKA RUCHU PIR (NAJWYŻSZY PRIORYTET) ---
  // Przekazujemy flagę konfiguracji ORAZ offline mode (choć timeout jest wewnątrz funkcji)
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
  // Działa tylko jeśli nie jesteśmy Offline
  if (isNtpSyncPending && !isOfflineMode) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10) && timeinfo.tm_year > (2023 - 1900)) {
        Serial.println("\nTime synchronized successfully! (from loop)");
        isNtpSyncPending = false; 
    }
  }

  // --- ZAPIS LOKALIZACJI ---
  if (isLocationSavePending) {
    Serial.println("LOOP: Zapisywanie lokalizacji...");
    locationManager.saveLocationToPreferences();
    isLocationSavePending = false; 
  }
  
  // --- AUTO-RECONNECT SYSTEM ---
  // Blokujemy w trybie Offline
  if (!isOfflineMode) {
      static unsigned long lastWiFiSystemCheck = 0;
      if (millis() - lastWiFiSystemCheck > WIFI_STATUS_CHECK_INTERVAL) { 
        lastWiFiSystemCheck = millis();
        checkWiFiConnection();
        handleWiFiLoss();
        handleBackgroundReconnect();
      }
  }
  
  // --- TRIGGERY WIFI CONFIG (LONG PRESS) ---
  if (checkWiFiLongPress(tft)) {
    Serial.println("🌐 LONG PRESS - Entering WiFi config!");
    enterWiFiConfigMode(tft);
    return;
  }
  
  // --- OBSŁUGA KOMEND SERIAL ---
  if (Serial.available()) {
    char command = Serial.read();
    
    if (WiFi.status() == WL_CONNECTED) {
        if (command == 'f' || command == 'F') getForecast();
        else if (command == 'w' || command == 'W') getWeather();
        else if (command == 'x' || command == 'X') generateWeeklyForecast();
        else if (command == 'u' || command == 'U') {
            Serial.println("🧪 TEST: Wymuszam sprawdzenie aktualizacji z GitHuba...");
            GithubUpdateManager updateMgr;
            updateMgr.checkForUpdate(); 
            Serial.println("🏁 Koniec testu aktualizacji.");
        }
    } else {
        if (strchr("fwxuFWXU", command)) {
            Serial.println("❌ Ignoruję komendę: Brak połączenia WiFi (lub Tryb Offline)");
        }
    }
  }

  //DEBUG
  isOfflineMode = true; // to delete

  // --- ZARZĄDZANIE EKRANAMI ---
  // Pozwalamy na działanie ScreenManagera w trybie Offline (żeby wymusił Ekran 4)
  if (!isWiFiLost() || isOfflineMode) {
    updateScreenManager();
  }

  // --- BLOKUJEMY POBIERANIE POGODY W TRYBIE OFFLINE ---
  if (!isOfflineMode) {

      // === WEEKLY FORECAST UPDATE ===
      unsigned long weeklyInterval = weeklyErrorModeGlobal ? WEEKLY_UPDATE_ERROR : WEEKLY_UPDATE_INTERVAL;
      if (millis() - lastWeeklyUpdate >= weeklyInterval) {
        lastWeeklyUpdate = millis();
        if (WiFi.status() == WL_CONNECTED) {
          if (generateWeeklyForecast()) weeklyErrorModeGlobal = false;
          else weeklyErrorModeGlobal = true;
        }
      }

      // --- WEATHER UPDATE ---
      unsigned long weatherInterval = weatherErrorModeGlobal ? WEATHER_UPDATE_ERROR : WEATHER_UPDATE_NORMAL;
      if (millis() - lastWeatherCheckGlobal >= weatherInterval) {
        if (WiFi.status() == WL_CONNECTED) {
          getWeather();
          if (weather.isValid) {
            weatherErrorModeGlobal = false;
            if (getScreenManager().getCurrentScreen() == SCREEN_CURRENT_WEATHER) {
               // Opcjonalne odświeżenie
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
          if (forecast.isValid) forecastErrorModeGlobal = false;
          else forecastErrorModeGlobal = true;
        }
        lastForecastCheckGlobal = millis();
      }
  } // Koniec if (!isOfflineMode)

  // --- ODŚWIEŻANIE ZAWARTOŚCI EKRANU ---
  static ScreenType previousScreen = SCREEN_IMAGE;
  static unsigned long lastDisplayUpdate = 0;
  
  if (!isWiFiLost() || isOfflineMode) {
    ScreenType currentScreen = getScreenManager().getCurrentScreen();
    
    if (currentScreen != previousScreen) {
      switchToNextScreen(tft);
      previousScreen = currentScreen;
      lastDisplayUpdate = millis();
    }
    // Jeśli ekran ten sam, odświeżaj zegar/dane co sekundę
    else if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
      
      // 1. PRZYPADEK: Normalny tryb Online (Ekran główny pogody)
      if (!isOfflineMode && currentScreen == SCREEN_CURRENT_WEATHER && !isWiFiConfigActive()) {
          if (WiFi.status() == WL_CONNECTED) {
            displayTime(tft);
          }
          if (weather.isValid) {
          //displayCurrentWeather(tft);
          } else {
            tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("BRAK DANYCH!", tft.width() / 2, 50);
          }
      }
      
      // 2. PRZYPADEK: Tryb Offline (Ekran Sensorów)
      // Sprawdzamy: czy Offline ORAZ czy licznik parzysty (czyli wyświetlamy sensory)
      else if (isOfflineMode && currentScreen == SCREEN_LOCAL_SENSORS) {
          displayTime(tft); 
      }

      // Resetujemy licznik czasu dla obu przypadków
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
    
    lastWeatherCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    lastForecastCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    lastWeeklyUpdate = millis() - WEEKLY_UPDATE_INTERVAL; 
}
