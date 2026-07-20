#include "wifi/wifi_touch_interface.h"
#include "wifi/wifi_touch_interface_ui.h"
#include "wifi/wifi_touch_interface_internal.h"

#include <SPI.h>
#include "managers/ScreenManager.h"
#include "managers/MotionSensorManager.h" 
#include "config/location_config.h"
#include "weather/weather_data.h"   
#include "weather/forecast_data.h"  
#include "display/display_utils.h"
#include "wifi/offline_mode_pref.h"
#include "display/display_pressure.h" // Niezbędne do odczytu i przełączania showPressureAtSeaLevel
#include "weather/open_meteo_api.h"   // Do pobrania poprawnej wartości weather.pressure po kliknięciu
#include "config/hardware_config.h"  

#ifndef DEBUG_LONG_PRESS
#define DEBUG_LONG_PRESS 1
#endif

extern bool isNtpSyncPending;
extern bool isLocationSavePending;
extern bool weatherErrorModeGlobal;  
extern bool forecastErrorModeGlobal; 
extern bool weeklyErrorModeGlobal;
extern bool isOfflineMode;

extern bool isImageDownloadInProgress; 

// Colors
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define DARK_GREEN 0x0340  
#define ORANGE 0xFD20  
#define DARK_BLUE 0x001F  
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GRAY    0x8410
#define DARKGRAY 0x4208
#define COLOR_BACKGROUND 0x0000  

extern TFT_eSPI tft; 

Preferences preferences;
static bool wifiPrefsInitialized = false;
static void ensureWiFiPrefs() {
  if (!wifiPrefsInitialized) {
    preferences.begin("wifi", false);
    wifiPrefsInitialized = true;
  }
}

extern void onWiFiConnectedTasks();

// WiFi management
String defaultSSID = "YourDefaultWiFi";
String defaultPassword = "YourDefaultPassword";
String currentSSID = "";
String currentPassword = "";

WiFiState currentState = STATE_CONNECTING;
int selectedNetworkIndex = -1;
String enteredPassword = "";
bool capsLock = false;
bool specialMode = false;  

// Long press detection
unsigned long touchStartTime = 0;
bool touchActive = false;
bool longPressDetected = false;
unsigned long configModeStartTime = 0;

// WiFi connection monitoring
unsigned long lastWiFiCheck = 0;
unsigned long wifiLostTime = 0;
bool wifiWasConnected = false;
bool wifiLostDetected = false;
unsigned long lastReconnectAttempt = 0;
bool backgroundReconnectActive = false;

bool reconnectAttemptInProgress = false;
unsigned long reconnectStartTime = 0;

// Network data
int networkCount = 0;
String networkNames[20];
int networkRSSI[20];
bool networkSecure[20];

// Password visibility toggle
bool showPassword = false;

// Location selection variables
LocationMenuState currentMenuState = MENU_MAIN;
int currentLocationIndex = 0;
int selectedCityIndex = 0; 

const char* mainMenuOptions[] = {"Szczecin", "Poznan", "Zlocieniec", "Wlasny GPS"};

String customLatitude = "53.4289";
String customLongitude = "14.5530";
bool editingLatitude = true; 
int coordinatesCursorPos = 0;

void handleTouchInput(int16_t x, int16_t y);
void handleKeyboardTouch(int16_t x, int16_t y);

void initWiFiTouchInterface() {
  Serial.println("=== Initializing WiFi Touch Interface ===");
  
  preferences.begin("wifi", false);
  Serial.println("Hardware initialized");
  
  defaultSSID = preferences.getString("ssid", "YourWiFi");
  defaultPassword = preferences.getString("password", "password");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi already connected from main.cpp");
    Serial.printf("Current network: %s\n", WiFi.SSID().c_str());
    
    String currentSSID = WiFi.SSID();
    if (currentSSID != defaultSSID && currentSSID.length() > 0) {
      Serial.println("Updating saved WiFi credentials to match current connection");
      preferences.putString("ssid", currentSSID);
    }
    
    currentState = STATE_CONNECTED;
  } else {
    Serial.println("No WiFi connection, trying saved credentials...");
    wifiTouchUI_drawStatusMessage(tft, "Laczenie z WiFi...");
    
    isImageDownloadInProgress = true; 
    
    WiFi.begin(defaultSSID.c_str(), defaultPassword.c_str());
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      yield(); 
      timeout++;
      wifiTouchUI_drawStatusMessage(tft, "Laczenie... " + String(timeout) + "/20");
    }
    
    isImageDownloadInProgress = false; 
    
    if (WiFi.status() == WL_CONNECTED) {
      currentState = STATE_CONNECTED;
      
      extern ScreenManager& getScreenManager();
      getScreenManager().resetScreenTimer();
      Serial.println("Connected to saved WiFi! Screen rotation timer reset for 60s cycle.");
    } else {
      Serial.println("Saved WiFi failed, scanning networks...");

      backgroundReconnectActive = true; 
      lastReconnectAttempt = millis();  
      Serial.println("Background reconnect activated after sleep mode failure");
      
      currentState = STATE_SCAN_NETWORKS;
      
      yield(); 
      scanNetworks();
      drawNetworkList(tft);

      tft.fillRect(10, 210, 300, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setTextSize(1);
      tft.setCursor(15, 220);
      tft.println("Brak WiFi - Polacz co 19s lub wybierz siec");
    }
  }
}

void handleWiFiTouchLoop(TFT_eSPI& tft) {
  uint16_t x, y;
  
  checkWiFiConnection();
  handleWiFiLoss();
  handleBackgroundReconnect();
  handleConfigModeTimeout();
  
  if (currentState != STATE_CONFIG_MODE) {
    handleLongPress(tft);
  } else {
    touchActive = false;
    longPressDetected = false;
  }
  
  if (tft.getTouch(&x, &y)) {

    extern MotionSensorManager& getMotionSensorManager();
    if (getMotionSensorManager().isGhostTouchProtectionActive()) {
        Serial.println("👻 Ghost Touch Detected & Ignored (Voltage spike)");
        return; 
    }
    
    extern ScreenManager& getScreenManager();
    getScreenManager().resetScreenTimer();
  
    getMotionSensorManager().handleMotionInterrupt(); 

    Serial.printf("Calibrated Touch: X=%d, Y=%d\n", x, y);
    handleTouchInput((int16_t)x, (int16_t)y);
    delay(150); 
  }
}

bool checkWiFiLongPress(TFT_eSPI& tft) {
  handleLongPress(tft);
  
  if (longPressDetected) {
    longPressDetected = false; 
    return true;
  }
  return false;
}

void enterWiFiConfigMode(TFT_eSPI& tft) {
  enterConfigMode();
}

bool isWiFiConfigActive() {
  return (currentState == STATE_CONFIG_MODE ||
          currentState == STATE_SCAN_NETWORKS ||
          currentState == STATE_ENTER_PASSWORD ||
          currentState == STATE_SELECT_LOCATION ||
          currentState == STATE_ENTER_COORDINATES);
}

void exitWiFiConfigMode() {
  currentState = STATE_CONNECTED;
}
 
void scanNetworks() {
  drawStatusMessage(tft, "Szukam sieci...");
  
  isImageDownloadInProgress = true; 
  WiFi.disconnect(true, true); 
  delay(200);
  yield(); 
  WiFi.mode(WIFI_STA);
  delay(200);
  yield(); 

  networkCount = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", networkCount);
  
  isImageDownloadInProgress = false; 
  
  if (networkCount == 0) {
    drawStatusMessage(tft, "No networks found");
    delay(2000);
    return;
  }
  
  for (int i = 0; i < networkCount && i < 20; i++) {
    networkNames[i] = WiFi.SSID(i);
    networkRSSI[i] = WiFi.RSSI(i);
    networkSecure[i] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    Serial.printf("Network %d: %s (%d dBm) %s\n", i, networkNames[i].c_str(), networkRSSI[i], networkSecure[i] ? "SECURE" : "OPEN");
  }
}

void connectToWiFi() {
  drawStatusMessage(tft, "Laczenie z " + currentSSID + "...");
  Serial.println("Connecting to: " + currentSSID);
  
  isImageDownloadInProgress = true; 
  currentPassword = enteredPassword;
  WiFi.begin(currentSSID.c_str(), currentPassword.c_str());
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    yield(); 
    Serial.print(".");
    timeout++;
    drawStatusMessage(tft, "Laczenie... " + String(timeout) + "/30");
  }
  
  isImageDownloadInProgress = false; 
  
  if (WiFi.status() == WL_CONNECTED) {
    ensureWiFiPrefs();
    preferences.putString("ssid", currentSSID);
    preferences.putString("password", currentPassword);

    if (isOfflineMode) {
      isOfflineMode = false;
      saveOfflineModePref(false);
      Serial.println("[WiFi] connected -> leaving OFFLINE mode");
    }
    
    extern ScreenManager& getScreenManager();
    getScreenManager().resetScreenTimer();
    
    onWiFiConnectedTasks();

    currentState = STATE_CONNECTED;
    drawConnectedScreen(tft);
    Serial.println("\nConnected successfully! Screen timer reset for 60s cycle.");

    delay(1000);
    tft.fillScreen(COLOR_BACKGROUND);
    extern void forceScreenRefresh(TFT_eSPI& tft);
    forceScreenRefresh(tft);

  } else {
    Serial.println("\nConnection failed!");
    tft.fillScreen(RED);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 100);
    tft.println("BLAD!");
    tft.setCursor(10, 130);
    tft.println("Dotknij aby powtorzyc");
    
    delay(DELAY_SUCCESS_DISPLAY);
    currentState = STATE_SCAN_NETWORKS;
    drawNetworkList(tft);
  }
}

extern void forceScreenRefresh(TFT_eSPI& tft);

void handleLongPress(TFT_eSPI& tft) {
  if (currentState != STATE_CONNECTED) {
      touchActive = false; 
      return; 
  }

  uint16_t x, y;
  bool currentTouch = tft.getTouch(&x, &y);

#if DEBUG_LONG_PRESS
  static unsigned long lastProgressLogMs = 0;
  static unsigned long lastLostLogMs = 0;
#endif

  static unsigned long lastTouchSeenMs = 0;
  const unsigned long TOUCH_GLITCH_TOLERANCE_MS = 120;
  const unsigned long TAP_REFRESH_MAX_MS = 700;
  
  // ZMIANA: Dodano statyczne zmienne startowe touch do precyzyjnego pozycjonowania kliknięcia
  static uint16_t touchStartX = 0;
  static uint16_t touchStartY = 0;

  if (currentTouch) {
    lastTouchSeenMs = millis();
    extern ScreenManager& getScreenManager();
    getScreenManager().resetScreenTimer();
  }
  
  if (currentTouch && !touchActive) {
    // Zapamiętaj współrzędne początku kliknięcia
    touchStartX = x;
    touchStartY = y;

    touchStartTime = millis();
    touchActive = true;
    longPressDetected = false;
#if DEBUG_LONG_PRESS
    Serial.printf("[LP] START x=%u y=%u state=%d\n", x, y, currentState);
    lastProgressLogMs = 0;
#endif
  }
  else if (!currentTouch && touchActive) {
    if ((millis() - lastTouchSeenMs) < TOUCH_GLITCH_TOLERANCE_MS) {
      return;
    }

    unsigned long elapsed = millis() - touchStartTime;

#if DEBUG_LONG_PRESS
    unsigned long now = millis();
    if (elapsed < WIFI_LONG_PRESS_TIME && (now - lastLostLogMs) > 150) {
      lastLostLogMs = now;
      Serial.printf("[LP] LOST/END elapsed=%lu ms (<%u) state=%d\n", elapsed, (unsigned)WIFI_LONG_PRESS_TIME, currentState);
    }
#endif
    
    if (elapsed >= 200 && elapsed < WIFI_LONG_PRESS_TIME) {
      if (elapsed <= TAP_REFRESH_MAX_MS) {
        
        // ==============================================================
        // ZMIANA: PRZYCHWYCENIE KLIKNIĘCIA W PRZYCISK CIŚNIENIA NA EKRANIE CIŚNIENIA
        // ==============================================================
        extern ScreenManager& getScreenManager();
        if (getScreenManager().getCurrentScreen() == SCREEN_PRESSURE) {
            // Przycisk znajduje się na X: 15 do 185, Y: 202 do 232 (dodano tolerancję 5px)
            if (touchStartX >= 10 && touchStartX <= 190 && touchStartY >= 195 && touchStartY <= 235) {
                Serial.println("🎯 [PIR] Kliknięto przycisk zmiany wysokości ciśnienia!");
                
                // 1. Zmiana stanu
                showPressureAtSeaLevel = !showPressureAtSeaLevel;
                
                // 2. === NOWOŚĆ: TRWAŁY ZAPIS DO PAMIĘCI FLASH ===
                Preferences prefs;
                prefs.begin("settings", false); // false = tryb zapisu
                prefs.putBool("msl_mode", showPressureAtSeaLevel);
                prefs.end();
                Serial.println("💾 Zapisano trwale tryb ciśnienia: " + String(showPressureAtSeaLevel ? "MSL" : "GRUNT"));
                // ================================================
                
                // 3. Od razu zaktualizuj odczyt bieżący stacji
                if (isOpenMeteoDataValid()) {
                    const float* onlineData = getOpenMeteoPressureHistory();
                    weather.pressure = onlineData[11];
                }
                
                forceScreenRefresh(tft); // Przerysuj ekran w nowym trybie
                touchActive = false;
                longPressDetected = false;
                return; // PRZERYWAMY pętlę, aby zapobiec przełączeniu ekranu TFT na kolejny!
            }
        }
        // ==============================================================

        Serial.println("Touch released on Main Screen - Refreshing...");
        forceScreenRefresh(tft);
      }
    }
    
    touchActive = false;
    longPressDetected = false;
  }
  else if (currentTouch && touchActive && !longPressDetected) {
    
    if (millis() - touchStartTime >= WIFI_LONG_PRESS_TIME) { 
      longPressDetected = true;
#if DEBUG_LONG_PRESS
      Serial.printf("[LP] TRIGGER elapsed=%lu ms state=%d\n", millis() - touchStartTime, currentState);
#endif
      Serial.println("LONG PRESS - Config Mode!");
    }
    else {
      unsigned long elapsed = millis() - touchStartTime;
      
      if (elapsed >= 200 && elapsed < WIFI_LONG_PRESS_TIME) {
        int progress = map(elapsed, 200, WIFI_LONG_PRESS_TIME, 0, 100);
        if (progress < 0) progress = 0;
        if (progress > 100) progress = 100;

#if DEBUG_LONG_PRESS
        if ((millis() - lastProgressLogMs) > 250) {
          lastProgressLogMs = millis();
          Serial.printf("[LP] progress=%d%% elapsed=%lu ms state=%d touch=%d\n", progress, elapsed, currentState, (int)currentTouch);
        }
#endif
        
        tft.fillRect(10, 10, 300, 20, BLACK);
        tft.drawRect(10, 10, 300, 20, WHITE);
        
        if (progress > 0) {
            tft.fillRect(12, 12, (progress * 296) / 100, 16, YELLOW);
        }
        
        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
        tft.drawString("Trzymaj...", 130, 35);
      }
    }
  }
}

void handleConfigModeTimeout() {
  if (currentState == STATE_CONFIG_MODE) {
    unsigned long elapsed = millis() - configModeStartTime;

    if (elapsed >= WIFI_CONFIG_MODE_TIMEOUT) { 
      Serial.println("Config mode timeout - returning to normal operation");
      currentState = STATE_CONNECTED;
      tft.fillScreen(COLOR_BACKGROUND);
    } else {
      static unsigned long lastUpdate = 0;
      if (millis() - lastUpdate > 1000) { 
        lastUpdate = millis();
        int remaining = (WIFI_CONFIG_MODE_TIMEOUT - elapsed) / 1000;

        tft.fillRect(240, 222, 72, 12, DARKGRAY);
        tft.setTextColor(YELLOW, DARKGRAY);
        tft.setTextSize(1);
        tft.setCursor(242, 224);
        tft.printf("Exit:%d", remaining);
      }
    }
  }
}

void enterConfigMode() {
  Serial.println("Entering CONFIG MODE for 120 seconds");
  currentState = STATE_CONFIG_MODE;
  configModeStartTime = millis();
  WiFi.disconnect();
  scanNetworks();
  drawConfigModeScreen();
}

bool isWiFiLost() {
  return wifiLostDetected || (currentState == STATE_SCAN_NETWORKS && backgroundReconnectActive);
}

void handleBackgroundReconnect() {
  if (!backgroundReconnectActive || currentState != STATE_SCAN_NETWORKS) {
    return;
  }

  if (reconnectAttemptInProgress) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Background reconnect SUCCESS! Exiting scan mode.");
      
      isImageDownloadInProgress = false; 
      
      backgroundReconnectActive = false;
      reconnectAttemptInProgress = false;
      currentState = STATE_CONNECTED;
      wifiLostDetected = false;
      wifiLostTime = 0;
      
      extern ScreenManager& getScreenManager();
      getScreenManager().resetScreenTimer();
      
      onWiFiConnectedTasks();
      
      Serial.println("Reconnected! Resuming normal operation.");
      return;
    }
    
    if (millis() - reconnectStartTime > WIFI_CONNECTION_TIMEOUT) { 
      Serial.println("Background reconnect attempt timed out, will retry in 19s...");
      
      isImageDownloadInProgress = false; 
      
      reconnectAttemptInProgress = false; 
      lastReconnectAttempt = millis(); 
      WiFi.disconnect(); 
    }
    
    return;
  }

  if (isImageDownloadInProgress) {
    Serial.println("⏸️ WiFi auto-reconnect SKIPPED - image download in progress");
    return; 
  }
  
  if (millis() - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    
    if (savedSSID.length() > 0) {
      Serial.printf("Background auto-reconnect to: %s (scan mode)\n", savedSSID.c_str());
      
      isImageDownloadInProgress = true; 
      reconnectAttemptInProgress = true; 
      reconnectStartTime = millis();     
      
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str()); 
      
      tft.fillRect(10, 210, 300, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setTextSize(1);
      tft.setCursor(15, 220);
      tft.printf("Trying saved WiFi... or select network");
      
    } else {
      Serial.println("No saved WiFi credentials, resetting timer");
      lastReconnectAttempt = millis();
    }
  }

  if (!reconnectAttemptInProgress) {
    static unsigned long lastUpdateTime = 0;
    if (millis() - lastUpdateTime > WIFI_UI_UPDATE_INTERVAL) { 
      lastUpdateTime = millis();
      unsigned long elapsed = millis() - lastReconnectAttempt;
      int nextAttempt = (WIFI_RECONNECT_INTERVAL - elapsed) / 1000;
      
      if (nextAttempt > 0 && nextAttempt <= 19) {
        tft.fillRect(240, 193, 75, 15, BLUE); 
        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setCursor(248, 197); 
        tft.printf("Next: %ds", nextAttempt);
      }
    }
  }
}

bool wifiTouch_isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void checkWiFiConnection() {
  if (millis() - lastWiFiCheck > WIFI_STATUS_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    
    if (currentState == STATE_CONNECTED) {
      if (!isConnected && wifiWasConnected) {
        if (!wifiLostDetected) {
          wifiLostDetected = true;
          wifiLostTime = millis();
          lastReconnectAttempt = millis();
          Serial.printf("WiFi LOST! Starting %d-second countdown...\n", WIFI_LOSS_TIMEOUT/1000);
          if (!touchActive) {
              drawConnectedScreen(tft); 
          }
        }
      } else if (isConnected && wifiLostDetected) {
        wifiLostDetected = false;
        Serial.println("WiFi RECONNECTED! Canceling auto-reconnect.");
        if (!touchActive) {
            drawConnectedScreen(tft);
        }
      }
    }
    
    wifiWasConnected = isConnected;
  }
}

void handleWiFiLoss() {
  if (!wifiLostDetected || currentState != STATE_CONNECTED) {
    return;
  }

  unsigned long elapsed = millis() - wifiLostTime;

  if (reconnectAttemptInProgress) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Auto-reconnect SUCCESS!");
      
      isImageDownloadInProgress = false; 
      
      wifiLostDetected = false;
      reconnectAttemptInProgress = false;
      drawConnectedScreen(tft); 
      
      extern ScreenManager& getScreenManager();
      getScreenManager().resetScreenTimer();
      return;
    }
    
    if (millis() - reconnectStartTime > WIFI_CONNECTION_TIMEOUT) { 
      Serial.println("Auto-reconnect attempt timed out, will retry in 19s...");
      
      isImageDownloadInProgress = false; 
      
      reconnectAttemptInProgress = false; 
      lastReconnectAttempt = millis();    
      WiFi.disconnect();                  
    }
    return;
  }

  if (isImageDownloadInProgress) {
    Serial.println("⏸️ WiFi loss reconnect SKIPPED - image download in progress");
    return; 
  }
  
  if (elapsed < WIFI_LOSS_TIMEOUT && millis() - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    
    if (savedSSID.length() > 0) {
      Serial.printf("Auto-reconnect attempt to: %s (grace period)\n", savedSSID.c_str());
      
      isImageDownloadInProgress = true;  
      reconnectAttemptInProgress = true; 
      reconnectStartTime = millis();     
      
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str()); 
    } else {
      Serial.println("No saved WiFi credentials, resetting timer");
      lastReconnectAttempt = millis();
    }
  }

  if (elapsed >= WIFI_LOSS_TIMEOUT) {
    Serial.printf("%d seconds elapsed. Grace period over. Starting network scan...\n", WIFI_LOSS_TIMEOUT/1000);

    if (reconnectAttemptInProgress) {
       unsigned long finalWaitStart = millis();
       while(millis() - finalWaitStart < 3000 && WiFi.status() != WL_CONNECTED) { 
         delay(100); 
       }
       
       if (WiFi.status() == WL_CONNECTED) {
          Serial.println("FINAL auto-reconnect SUCCESS!");
          
          isImageDownloadInProgress = false; 
          
          wifiLostDetected = false;
          reconnectAttemptInProgress = false;
          drawConnectedScreen(tft);
          return;
       }
    }
    
    isImageDownloadInProgress = false; 
    
    Serial.println("All reconnect attempts failed. Starting network scan...");
    wifiLostDetected = false;
    reconnectAttemptInProgress = false;
    backgroundReconnectActive = true; 
    lastReconnectAttempt = millis();  
    currentState = STATE_SCAN_NETWORKS;
    
    WiFi.disconnect();
      
    scanNetworks();
    drawNetworkList(tft);
      
    tft.fillRect(10, 210, 300, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setTextSize(1);
    tft.setCursor(15, 220);
    tft.println("Brak WiFi - Polacz co 19s lub wybierz siec");
      
  } else {
    static unsigned long lastCountdownUpdate = 0;
    if (millis() - lastCountdownUpdate > 1000) {
      lastCountdownUpdate = millis();
      if (!touchActive) {
        drawConnectedScreen(tft); 
      }
    }
  }
}

void enterLocationSelectionMode(TFT_eSPI& tft) {
  Serial.println("Entering LOCATION SELECTION MODE");
  currentState = STATE_SELECT_LOCATION;
  currentMenuState = MENU_MAIN; 
  currentLocationIndex = 0;
  selectedCityIndex = 0;
  drawLocationScreen(tft);
}

void enterCoordinatesMode(TFT_eSPI& tft) {
  Serial.println("Entering CUSTOM COORDINATES MODE");
  currentState = STATE_ENTER_COORDINATES;
  
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  customLatitude = String(currentLoc.latitude, 4);
  customLongitude = String(currentLoc.longitude, 4);
  editingLatitude = true;
  coordinatesCursorPos = 0;
  
  drawCoordinatesScreen(tft);
}