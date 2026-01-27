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

// === DODANO TEN IMPORT, ŻEBY NAPRAWIĆ BŁĄD ===
#include "display/sensors_display.h" 

// --- SENSORY ---
#include "sensors/motion_sensor.h"
#include "sensors/dht22_sensor.h" 

// --- WIFI TOUCH INTERFACE ---
#include "wifi/wifi_touch_interface.h"

#ifdef USE_SHT31
  #include "sensors/sht31_sensor.h"
#else
  #include "sensors/dht22_sensor.h"
#endif

// === FLAGA BLOKADY WiFi PODCZAS POBIERANIA OBRAZKA ===
bool isImageDownloadInProgress = false;

// === FLAGA TRYBU OFFLINE (BEZ WIFI) ===
bool isOfflineMode = false; 

// --- EXTERNAL FUNCTION DECLARATIONS ---

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

uint16_t activeTouchCalibration[5] = {0, 0, 0, 0, 0};

void setup() {
  Serial.begin(115200);
  delay(DELAY_STABILIZATION); 
  
// ============================================================
  // 1. LOGIKA KALIBRACJI (OTA SAFE) - Z DEBUGOWANIEM
  // ============================================================
  {
      Serial.println("\n--- DEBUG START: KALIBRACJA ---");
      Preferences prefs;
      // Używamy tej samej nazwy przestrzeni co zawsze
      bool success = prefs.begin("touch_cal", false); 
      
      if (!success) {
          Serial.println("❌ BŁĄD KRYTYCZNY: Nie udało się otworzyć pamięci Preferences!");
      }

      // Sprawdzamy czy klucz istnieje
      bool hasKey = prefs.isKey("cal_done");
      Serial.print("❓ Czy klucz 'cal_done' istnieje w pamieci? -> ");
      Serial.println(hasKey ? "TAK" : "NIE");

      if (hasKey) {
          Serial.println("📥 ŚCIEŻKA A: Wczytywanie kalibracji z pamięci (NVS)...");
          
          activeTouchCalibration[0] = prefs.getUShort("c0", 0);
          activeTouchCalibration[1] = prefs.getUShort("c1", 0);
          activeTouchCalibration[2] = prefs.getUShort("c2", 0);
          activeTouchCalibration[3] = prefs.getUShort("c3", 0);
          activeTouchCalibration[4] = prefs.getUShort("c4", 0);
          
          Serial.print("📊 Odczytane wartości: ");
          Serial.printf("[%d, %d, %d, %d, %d]\n", 
              activeTouchCalibration[0], activeTouchCalibration[1], 
              activeTouchCalibration[2], activeTouchCalibration[3], 
              activeTouchCalibration[4]);
              
          if (activeTouchCalibration[0] == 0 && activeTouchCalibration[1] == 0) {
             Serial.println("⚠️ UWAGA: Wartości wynoszą 0! Coś poszło nie tak z zapisem wcześniej.");
          }
      } 
      else {
          Serial.println("⚙️ ŚCIEŻKA B: Brak danych w pamięci. Pobieranie z secrets.h...");
          
          const uint16_t* factoryCal = getFactoryCalibrationFromSecrets();

          if (factoryCal != nullptr) {
              Serial.println("✅ Znaleziono dane w secrets.h - Zapisuję do pamięci...");
              for (int i = 0; i < 5; i++) {
                  activeTouchCalibration[i] = factoryCal[i];
                  // Debug zapisu
                  String key = "c" + String(i);
                  size_t saved = prefs.putUShort(key.c_str(), activeTouchCalibration[i]);
                  if (saved == 0) Serial.println("❌ Błąd zapisu klucza: " + key);
              }
              
              prefs.putBool("cal_done", true); 
              Serial.println("💾 Kalibracja zapisana. Po restarcie powinna być w Ścieżce A.");
          } else {
              Serial.println("❌ BŁĄD: getFactoryCalibrationFromSecrets zwrócił NULL!");
          }
      }
      prefs.end();
      Serial.println("--- DEBUG END: KALIBRACJA ---\n");
  }
  // ============================================================

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

      // === JITTER ===
      {
         int jitterSeconds = random(0, FIRMWARE_UPDATE_JITTER + 1);
         Serial.printf("🎲 Jitter: Czekam %d sekund przed sprawdzeniem aktualizacji...\n", jitterSeconds);
         for(int i=0; i<jitterSeconds; i++) {
             delay(1000); 
             sysManager.loop(); 
         }
      }
      
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
  
  // Aplikacja kalibracji (z globalnej zmiennej)
  const uint16_t* cal = getTouchCalibration();
  if (cal) {
      tft.setTouch(const_cast<uint16_t*>(cal));
      Serial.println("✅ Touch calibration applied.");
  }
  else {
      Serial.println("⚠️ No touch calibration found");
  }

  tft.fillScreen(COLOR_BACKGROUND);

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
  Preferences prefs;
  prefs.begin("wifi", true); 
  String savedSSID = prefs.getString("ssid", "");
  String savedPassword = prefs.getString("password", "");
  prefs.end();
  
  String connectSSID = (savedSSID.length() > 0) ? savedSSID : WIFI_SSID;
  String connectPassword = (savedSSID.length() > 0) ? savedPassword : WIFI_PASSWORD;

  if (savedSSID.length() > 0) Serial.println("AUTO-CONNECT to saved WiFi: " + connectSSID);
  else Serial.println("Using default WiFi from secrets.h: " + connectSSID);

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
  
  initMotionSensor();

  // TYLKO SHT31 / DHT (Bez BME280)
  #ifdef USE_SHT31
    initSHT31(); 
  #else
    initDHT22(); 
  #endif

  locationManager.loadLocationFromPreferences();
  initWiFiTouchInterface();
  initNASAImageSystem();
  
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
  
  getScreenManager().resetScreenTimer();
  Serial.println("📱 Timer ekranu zresetowany - 10s do następnego przełączenia");
  
  Serial.println("=== STACJA POGODOWA GOTOWA ===");
}


void loop() {
  sysManager.loop(); 

  updateDisplayPowerState(tft, isWiFiConfigActive());

  #ifdef USE_SHT31
    updateSHT31(); 
  #else
    updateDHT22();
  #endif

  if (getDisplayState() == DISPLAY_SLEEPING) {
    delay(50); 
    return;
  }

  if (isWiFiConfigActive()) {
    handleWiFiTouchLoop(tft);
    return; 
  }

  if (isNtpSyncPending && !isOfflineMode) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10) && timeinfo.tm_year > (2023 - 1900)) {
        Serial.println("\nTime synchronized successfully! (from loop)");
        isNtpSyncPending = false; 
    }
  }

  if (isLocationSavePending) {
    Serial.println("LOOP: Zapisywanie lokalizacji...");
    locationManager.saveLocationToPreferences();
    isLocationSavePending = false; 
  }
  
  if (!isOfflineMode) {
      static unsigned long lastWiFiSystemCheck = 0;
      if (millis() - lastWiFiSystemCheck > WIFI_STATUS_CHECK_INTERVAL) { 
        lastWiFiSystemCheck = millis();
        checkWiFiConnection();
        handleWiFiLoss();
        handleBackgroundReconnect();
      }
  }
  
  if (checkWiFiLongPress(tft)) {
    Serial.println("🌐 LONG PRESS - Entering WiFi config!");
    enterWiFiConfigMode(tft);
    return;
  }
  
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

  if (!isWiFiLost() || isOfflineMode) {
    updateScreenManager();
  }

  if (!isOfflineMode) {
      unsigned long weeklyInterval = weeklyErrorModeGlobal ? WEEKLY_UPDATE_ERROR : WEEKLY_UPDATE_INTERVAL;
      if (millis() - lastWeeklyUpdate >= weeklyInterval) {
        lastWeeklyUpdate = millis();
        if (WiFi.status() == WL_CONNECTED) {
          if (generateWeeklyForecast()) weeklyErrorModeGlobal = false;
          else weeklyErrorModeGlobal = true;
        }
      }

      unsigned long weatherInterval = weatherErrorModeGlobal ? WEATHER_UPDATE_ERROR : WEATHER_UPDATE_NORMAL;
      if (millis() - lastWeatherCheckGlobal >= weatherInterval) {
        if (WiFi.status() == WL_CONNECTED) {
          getWeather();
          if (weather.isValid) {
            weatherErrorModeGlobal = false;
          } else {
            weatherErrorModeGlobal = true;
          }
        }
        lastWeatherCheckGlobal = millis();
      }

      unsigned long forecastInterval = forecastErrorModeGlobal ? WEATHER_UPDATE_ERROR : 1800000; 
      if (millis() - lastForecastCheckGlobal >= forecastInterval) {
        if (WiFi.status() == WL_CONNECTED) {
          getForecast();
          if (forecast.isValid) forecastErrorModeGlobal = false;
          else forecastErrorModeGlobal = true;
        }
        lastForecastCheckGlobal = millis();
      }
  } 

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
    else if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    
      // 1. Ekran Pogody (Tylko czas)
      if (currentScreen == SCREEN_CURRENT_WEATHER && !isWiFiConfigActive()) {
          if (WiFi.status() == WL_CONNECTED) {
            displayTime(tft);
          }
      }
      // 2. Ekran Sensorów (SHT31/DHT22) - POPRAWIONE
      // Obsługuje zarówno tryb Offline jak i Online
      else if (currentScreen == SCREEN_LOCAL_SENSORS) {
          displayLocalSensors(tft, true); // true = tylko odśwież liczby (bez migania)
          if (isOfflineMode)
          displayTime(tft);
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
    
    lastWeatherCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    lastForecastCheckGlobal = millis() - WEATHER_FORCE_REFRESH;
    lastWeeklyUpdate = millis() - WEEKLY_UPDATE_INTERVAL; 
}