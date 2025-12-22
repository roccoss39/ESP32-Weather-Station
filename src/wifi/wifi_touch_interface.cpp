#include "wifi/wifi_touch_interface.h"
#include <SPI.h>
#include "managers/ScreenManager.h"
#include "sensors/motion_sensor.h"
#include "managers/MotionSensorManager.h" 
#include "config/location_config.h"
#include "weather/weather_data.h"   
#include "weather/forecast_data.h"  

extern bool isNtpSyncPending;
extern bool isLocationSavePending;
extern bool weatherErrorModeGlobal;  // <-- DODAJ TĘ LINIĘ
extern bool forecastErrorModeGlobal; // <-- DODAJ TĘ LINIĘ
extern bool weeklyErrorModeGlobal;
extern bool isOfflineMode;

// Hardware pins - moved to hardware_config.h
#include "config/hardware_config.h"  // For TFT_BL pin definition

// Colors
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define DARK_GREEN 0x0340  // Professional dark green for connected screen
#define ORANGE 0xFD20  // Professional orange for WiFi lost screen
#define DARK_BLUE 0x001F  // Professional dark blue for WiFi lost screen
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GRAY    0x8410
#define DARKGRAY 0x4208
#define COLOR_BACKGROUND 0x0000  // Same as BLACK for clearing screen

// TFT instance - extern, defined in main.cpp
extern TFT_eSPI tft;
Preferences preferences;

extern void onWiFiConnectedTasks();

// WiFi management
String defaultSSID = "YourDefaultWiFi";
String defaultPassword = "YourDefaultPassword";
String currentSSID = "";
String currentPassword = "";

// Use WiFiState from header - remove duplicate enum
WiFiState currentState = STATE_CONNECTING;
int selectedNetworkIndex = -1;
String enteredPassword = "";
bool capsLock = false;
bool specialMode = false;  // For special characters keyboard

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

// NOWE ZMIENNE DO NIEBLOKUJĄCEGO AUTO-RECONNECT:
static bool reconnectAttemptInProgress = false;
static unsigned long reconnectStartTime = 0;

// Network data
int networkCount = 0;
String networkNames[20];
int networkRSSI[20];
bool networkSecure[20];

// Password visibility toggle
bool showPassword = false;

// Location selection variables - NOWY UPROSZCZONY SYSTEM
enum LocationMenuState {
  MENU_MAIN,           // Główne menu: Szczecin, Poznan, Zlocieniec, Wlasny GPS  
  MENU_DISTRICTS       // Lista dzielnic wybranego miasta
};

LocationMenuState currentMenuState = MENU_MAIN;
int currentLocationIndex = 0;
int selectedCityIndex = 0; // 0=Szczecin, 1=Poznan, 2=Zlocieniec, 3=Wlasny GPS

// Opcje głównego menu (4 opcje)
const char* mainMenuOptions[] = {"Szczecin", "Poznan", "Zlocieniec", "Wlasny GPS"};


// Custom coordinates variables (Szczecin Centrum - rzeczywiste współrzędne)
String customLatitude = "53.4289";
String customLongitude = "14.5530";
bool editingLatitude = true; // true=lat, false=lon
int coordinatesCursorPos = 0;

// Touch functions
// Old touch function declarations removed - using tft.getTouch() instead
void handleTouchInput(int16_t x, int16_t y);
void handleKeyboardTouch(int16_t x, int16_t y);

void initWiFiTouchInterface() {
  Serial.println("=== Initializing WiFi Touch Interface ===");
  
  // Initialize preferences
  preferences.begin("wifi", false);
  
  // Initialize backlight
  // pinMode(TFT_BL, OUTPUT);
  // digitalWrite(TFT_BL, HIGH);
  
  Serial.println("Hardware initialized");
  
  // Load saved credentials (but don't auto-connect if already connected)
  defaultSSID = preferences.getString("ssid", "YourWiFi");
  defaultPassword = preferences.getString("password", "password");
  
  // FIXED: Check if WiFi is already connected from main.cpp
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi already connected from main.cpp");
    Serial.printf("Current network: %s\n", WiFi.SSID().c_str());
    
    // Update saved credentials to match current connection
    String currentSSID = WiFi.SSID();
    if (currentSSID != defaultSSID && currentSSID.length() > 0) {
      Serial.println("Updating saved WiFi credentials to match current connection");
      preferences.putString("ssid", currentSSID);
      // Note: We can't get the current password, but that's ok
    }
    
    currentState = STATE_CONNECTED;
    // DON'T draw connected screen automatically - let normal screens show
  } else {
    // Only try to connect if WiFi is not already connected
    Serial.println("No WiFi connection, trying saved credentials...");
    drawStatusMessage(tft, "Laczenie z WiFi...");
    
    // Try saved WiFi credentials
    WiFi.begin(defaultSSID.c_str(), defaultPassword.c_str());
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
      drawStatusMessage(tft, "Laczenie... " + String(timeout) + "/20");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      currentState = STATE_CONNECTED;
      // DON'T draw connected screen automatically - let normal screens show
      
      // RESET screen rotation timer to show full 60s cycle before sleep
      extern ScreenManager& getScreenManager();
      getScreenManager().resetScreenTimer();
      Serial.println("Connected to saved WiFi! Screen rotation timer reset for 60s cycle.");
      Serial.println("Connected to saved WiFi!");
    } else {
      Serial.println("Saved WiFi failed, scanning networks...");

      // --- POPRAWKA SLEEP MODE - AKTYWUJ BACKGROUND RECONNECT ---
      backgroundReconnectActive = true; 
      lastReconnectAttempt = millis();  // Zresetuj timer, aby pierwsza próba była za 19s
      Serial.println("Background reconnect activated after sleep mode failure");
      
      currentState = STATE_SCAN_NETWORKS;
      scanNetworks();
      drawNetworkList(tft);

      // Żółty pasek na samym dole ekranu - FIXED for landscape 320x240
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
  
  // Check WiFi connection status every 2 seconds
  checkWiFiConnection();
  
  // Handle WiFi loss (60 second timeout)
  handleWiFiLoss();
  
  // Handle background reconnect when in scan mode
  handleBackgroundReconnect();
  
  // Handle config mode timeout
  handleConfigModeTimeout();
  
  // Only handle long press detection when NOT in config mode
  if (currentState != STATE_CONFIG_MODE) {
    handleLongPress(tft);
  } else {
    // Reset touch state when in config mode to avoid conflicts
    touchActive = false;
    longPressDetected = false;
  }
  
  // Use built-in calibrated TFT_eSPI touch function
  // Use built-in calibrated TFT_eSPI touch function
  if (tft.getTouch(&x, &y)) {

    // === GHOST TOUCH PROTECTION ===
    extern MotionSensorManager& getMotionSensorManager();
    if (getMotionSensorManager().isGhostTouchProtectionActive()) {
        Serial.println("👻 Ghost Touch Detected & Ignored (Voltage spike)");
        return; // IGNORUJ TEN DOTYK!
    }
    
    // Dopóki dotykasz ekranu, resetuj timer zmiany slajdów!
    extern ScreenManager& getScreenManager();
    getScreenManager().resetScreenTimer();

    // --- POPRAWKA UŚPIENIA (KROK 2) ---
    // Ręcznie zresetuj 10-sekundowy timer bezczynności,
    // ponieważ właśnie wykryliśmy PRAWDZIWĄ aktywność (dotyk).
    extern MotionSensorManager& getMotionSensorManager();
    getMotionSensorManager().handleMotionInterrupt(); // Ta funkcja resetuje timer
    // --- KONIEC POPRAWKI ---

    Serial.printf("Calibrated Touch: X=%d, Y=%d\n", x, y);
    handleTouchInput((int16_t)x, (int16_t)y);
    delay(150); // Debounce
  }
}

bool checkWiFiLongPress(TFT_eSPI& tft) {
  // Check for long press to enter WiFi config
  handleLongPress(tft);
  
  if (longPressDetected) {
    longPressDetected = false; // <-- POPRAWKA: Zużyj flagę po odczytaniu
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
          currentState == STATE_CONNECTING ||
          currentState == STATE_SELECT_LOCATION ||
          currentState == STATE_ENTER_COORDINATES);
}

void exitWiFiConfigMode() {
  currentState = STATE_CONNECTED;
  
  // Execute deferred location save (ROZWIĄZANIE PROBLEMU ROZŁĄCZENIA WiFi)
}
  // End of handleWiFiTouchLoop

void drawStatusMessage(TFT_eSPI& tft, String message) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);  // Middle Center
  tft.drawString(message, tft.width() / 2, tft.height() / 2);  // Wyśrodkowany
}

void drawConnectedScreen(TFT_eSPI& tft) {
  tft.fillScreen(DARK_BLUE);  // Tło pozostaje ciemnoniebieskie
  
  // --- ZMIANA TUTAJ: INTELIGENTNY NAGŁÓWEK ---
  if (wifiLostDetected || WiFi.status() != WL_CONNECTED) {
    // Jeśli WiFi utracone: Piszemy na POMARAŃCZOWO/CZERWONO "UTRACONO WIFI"
    tft.setTextColor(ORANGE); // Używamy zdefiniowanego wcześniej koloru ORANGE
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("UTRACONO POLACZENIE WIFI!", 160, 41);
  } else {
    // Jeśli wszystko OK: Piszemy na BIAŁO "POLACZONY!"
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(30, 50);
    tft.println("POLACZONY!");
  }
  // -------------------------------------------
  
  tft.setTextColor(WHITE); // Reset koloru dla reszty tekstów (IP, SSID)
  tft.setTextSize(1);
  tft.setCursor(10, 100);
  tft.println("Network: " + WiFi.SSID());
  tft.setCursor(10, 120);
  
  // Opcjonalnie: Jeśli nie ma WiFi, IP może być 0.0.0.0, można to też obsłużyć
  if (WiFi.status() == WL_CONNECTED) {
      tft.println("IP: " + WiFi.localIP().toString());
  } else {
      tft.println("IP: ---.---.---.---");
  }

  tft.setCursor(10, 140);
  tft.println("Signal: " + String(WiFi.RSSI()) + " dBm");
  
  // WiFi monitoring status (Twój czerwony pasek - to zostawiamy bez zmian)
  if (wifiLostDetected) {
    unsigned long elapsed = millis() - wifiLostTime;
    int remaining = (WIFI_LOSS_TIMEOUT - elapsed) / 1000;
    if (remaining > 0) {
      tft.fillRect(10, 160, 300, 20, RED);
      tft.setTextColor(WHITE);
      tft.setCursor(15, 165);
      tft.printf("Brak WiFi! Polacz za: %d sek (auto. proba: 9s)", remaining);
    }
  }
}
void scanNetworks() {
  drawStatusMessage(tft, "Szukam sieci...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  networkCount = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", networkCount);
  
  if (networkCount == 0) {
    drawStatusMessage(tft, "No networks found");
    delay(2000);
    return;
  }
  
  // Store network info
  for (int i = 0; i < networkCount && i < 20; i++) {
    networkNames[i] = WiFi.SSID(i);
    networkRSSI[i] = WiFi.RSSI(i);
    networkSecure[i] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    Serial.printf("Network %d: %s (%d dBm) %s\n", i, networkNames[i].c_str(), networkRSSI[i], networkSecure[i] ? "SECURE" : "OPEN");
  }
}

void drawNetworkList(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("Wybierz siec WiFi:");
  
  int yPos = 30;
  int maxNetworks = min(networkCount, 7);  // Reduced from 8 to 7 to make space for REFRESH button
  
  for (int i = 0; i < maxNetworks; i++) {
    // Highlight selected network
    if (i == selectedNetworkIndex) {
      tft.fillRect(0, yPos - 2, 240, 30, BLUE);
    }
    
    tft.setTextColor(WHITE);
    tft.setCursor(10, yPos + 5);
    
    // Security indicator
    if (networkSecure[i]) {
      tft.print("[*] ");
    } else {
      tft.print("[ ] ");
    }
    
    // Network name (truncate if too long)
    String displayName = networkNames[i];
    if (displayName.length() > 25) {
      displayName = displayName.substring(0, 25) + "...";
    }
    tft.print(displayName);
    
    // Signal strength
    tft.setCursor(200, yPos + 5);
    tft.print(networkRSSI[i]);
    
    yPos += 30;
  }
  
  // Refresh button - RIGHT SIDE MIDDLE, no overlap with WiFi names
  tft.fillRect(240, 120, 75, 30, BLUE);  // Right side, middle Y position
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(250, 130);
  tft.println("ODSWIEZ");
}

void drawPasswordScreen() {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("Wpisz haslo dla:");
  tft.setCursor(10, 25);
  tft.println(networkNames[selectedNetworkIndex]);
  
  // Password field - RESTORED ORIGINAL
  tft.drawRect(10, 50, 220, 25, WHITE);
  tft.fillRect(11, 51, 218, 23, BLACK);
  tft.setCursor(15, 58);
  
  // Show password with asterisks OR plain text based on toggle
  String displayPassword = "";
  if (showPassword) {
    displayPassword = enteredPassword; // Show actual password
  } else {
    for (int i = 0; i < enteredPassword.length(); i++) {
      displayPassword += "*"; // Show asterisks
    }
  }
  tft.print(displayPassword);
  
  // Show/Hide password toggle button
  tft.fillRect(240, 50, 75, 25, showPassword ? GREEN : GRAY);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(250, 58);
  if (showPassword) {
    tft.print("UKRYJ");
  } else {
    tft.print("POKAZ");
  }
  
  drawKeyboard();
}

void drawKeyboard() {
  String keys[4][12];
  
  if (specialMode) {
    // Special characters layout
    String specialKeys[4][12] = {
      {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+"},
      {"{", "}", "|", "\\", ":", "\"", "<", ">", "?", "~", "`", "="},
      {"[", "]", ";", "'", ",", ".", "/", "-", "€", "£", "¥", "§"},
      {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "±", "×"}
    };
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 12; j++) {
        keys[i][j] = specialKeys[i][j];
      }
    }
  } else {
    // QWERTY keyboard layout
    String normalKeys[4][12] = {
      {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "#"},
      {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "!", "?"},
      {"a", "s", "d", "f", "g", "h", "j", "k", "l", ".", "_", "-"},
      {"z", "x", "c", "v", "b", "n", "m", ",", ";", ":", "&", "*"}
    };
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 12; j++) {
        keys[i][j] = normalKeys[i][j];
      }
    }
  }
  
  int keyWidth = 25;  // Wider keys
  int keyHeight = 28; // Shorter to fit more
  int startY = 85;
  
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 12; col++) {
      if (keys[row][col] != "") {
        int x = col * (keyWidth + 1) + 2;  // Tighter spacing
        int y = row * (keyHeight + 1) + startY;
        
        // Draw key background
        tft.fillRect(x, y, keyWidth, keyHeight, DARKGRAY);
        tft.drawRect(x, y, keyWidth, keyHeight, WHITE);
        
        // Key label
        String keyLabel = keys[row][col];
        if (!specialMode && capsLock && keyLabel.length() == 1 && keyLabel[0] >= 'a' && keyLabel[0] <= 'z') {
          keyLabel.toUpperCase();
        }
        
        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setCursor(x + 8, y + 12);
        tft.print(keyLabel);
      }
    }
  }
  
  // Special keys row
  int specialY = startY + 4 * (keyHeight + 1);
  
  // Caps Lock / Special button
  if (specialMode) {
    tft.fillRect(2, specialY, 35, keyHeight, MAGENTA);
    tft.drawRect(2, specialY, 35, keyHeight, WHITE);
    tft.setTextColor(WHITE);
    tft.setCursor(8, specialY + 10);
    tft.print("ABC");
  } else {
    tft.fillRect(2, specialY, 35, keyHeight, capsLock ? BLUE : DARKGRAY);
    tft.drawRect(2, specialY, 35, keyHeight, WHITE);
    tft.setTextColor(WHITE);
    tft.setCursor(10, specialY + 10);
    tft.print("CAPS");
  }
  
  // Special characters button
  tft.fillRect(40, specialY, 35, keyHeight, specialMode ? CYAN : DARKGRAY);
  tft.drawRect(40, specialY, 35, keyHeight, WHITE);
  tft.setTextColor(WHITE);
  tft.setCursor(48, specialY + 10);
  tft.print("!@#");
  
  // Space bar
  tft.fillRect(80, specialY, 60, keyHeight, DARKGRAY);
  tft.drawRect(80, specialY, 60, keyHeight, WHITE);
  tft.setCursor(100, specialY + 10);
  tft.print("SPACE");
  
  // Delete
  tft.fillRect(145, specialY, 35, keyHeight, DARKGRAY);
  tft.drawRect(145, specialY, 35, keyHeight, WHITE);
  tft.setCursor(155, specialY + 10);
  tft.print("DEL");
  
  // Connect button - RESTORED WORKING COORDINATES
  tft.fillRect(185, specialY, 50, keyHeight, GREEN);
  tft.drawRect(185, specialY, 50, keyHeight, WHITE);
  tft.setCursor(195, specialY + 10);
  tft.print("CONN");
  
  // Back button - RESTORED WORKING COORDINATES  
  tft.fillRect(240, specialY, 75, keyHeight, RED);
  tft.drawRect(240, specialY, 75, keyHeight, WHITE);
  tft.setTextColor(WHITE);
  tft.setCursor(255, specialY + 10);
  tft.print("COFNIJ");
}

void connectToWiFi() {
  drawStatusMessage(tft, "Laczenie z " + currentSSID + "...");
  Serial.println("Connecting to: " + currentSSID);
  
  currentPassword = enteredPassword;
  WiFi.begin(currentSSID.c_str(), currentPassword.c_str());
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
    drawStatusMessage(tft, "Laczenie... " + String(timeout) + "/30");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Save successful credentials
    preferences.putString("ssid", currentSSID);
    preferences.putString("password", currentPassword);
    
    // RESET screen rotation timer for full 60s cycle before sleep
    extern ScreenManager& getScreenManager();
    getScreenManager().resetScreenTimer();
    
    onWiFiConnectedTasks();

    currentState = STATE_CONNECTED;
    drawConnectedScreen(tft);
    Serial.println("\nConnected successfully! Screen timer reset for 60s cycle.");
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

void handleLongPress(TFT_eSPI& tft) {
  uint16_t x, y;
  bool currentTouch = tft.getTouch(&x, &y);
  
  if (currentTouch) {
    // Jeśli trzymasz palec (nawet nieruchomo), blokuj zmianę ekranu
    extern ScreenManager& getScreenManager();
    getScreenManager().resetScreenTimer();
  }

  if (currentTouch && !touchActive) {
    // Touch started
    touchStartTime = millis();
    touchActive = true;
    longPressDetected = false;
    Serial.println("Touch started - long press detection active");
  }
  else if (!currentTouch && touchActive) {
    // Touch ended - clear progress bar if it was showing
    unsigned long elapsed = millis() - touchStartTime;
    if (elapsed >= 1000 && elapsed < 5000 && currentState == STATE_CONNECTED) {
      Serial.println("Touch ended - clearing progress bar");
      drawConnectedScreen(tft); // Clear progress bar
    }
    
    touchActive = false;
    longPressDetected = false;
    Serial.println("Touch ended");
  }
  else if (currentTouch && touchActive && !longPressDetected) {
    // Touch continues - check for long press
    if (millis() - touchStartTime >= WIFI_LONG_PRESS_TIME) { // 5 seconds
      longPressDetected = true;
      Serial.println("LONG PRESS DETECTED - Entering config mode!");
      
    }
    else {
      // Show progress indicator for long press
      unsigned long elapsed = millis() - touchStartTime;
      if (elapsed >= 1000 && elapsed < WIFI_LONG_PRESS_TIME) {
        int progress = map(elapsed, 1000, WIFI_LONG_PRESS_TIME, 0, 100);
        
        // Draw progress bar on connected screen
        tft.fillRect(10, 10, 300, 20, BLACK);
        tft.drawRect(10, 10, 300, 20, WHITE);
        tft.fillRect(12, 12, (progress * 296) / 100, 16, YELLOW);
        
        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setCursor(130, 35);
        tft.printf("Trzymaj %d...", (WIFI_LONG_PRESS_TIME - elapsed) / 1000 + 1);
      }
    }
  }
  
  // Clear progress bar when touch is released and long press wasn't triggered
  if (!currentTouch && !longPressDetected && touchActive) {
    // Clear any progress bar remnants
    if (currentState == STATE_CONNECTED) {
      extern void forceScreenRefresh(TFT_eSPI& tft);
      forceScreenRefresh(tft);
    }
  }
}

void handleConfigModeTimeout() {
  if (currentState == STATE_CONFIG_MODE) {
    unsigned long elapsed = millis() - configModeStartTime;
    
    if (elapsed >= WIFI_CONFIG_MODE_TIMEOUT) { // 120 seconds = 2 minutes
      Serial.println("Config mode timeout - returning to normal operation");
      currentState = STATE_CONNECTED;
      
      // Clear screen and let main.cpp take over
      tft.fillScreen(COLOR_BACKGROUND);
    }
    else {
      // Update countdown display
      static unsigned long lastUpdate = 0;
      if (millis() - lastUpdate > 1000) { // Update every second
        lastUpdate = millis();
        int remaining = (WIFI_CONFIG_MODE_TIMEOUT - elapsed) / 1000;
        
        tft.fillRect(250, 10, 65, 20, BLACK);
        tft.setTextColor(YELLOW);
        tft.setTextSize(1);
        tft.setCursor(250, 15);
        tft.printf("Exit: %d", remaining);
      }
    }
  }
}

void enterConfigMode() {
  Serial.println("Entering CONFIG MODE for 120 seconds");
  
  currentState = STATE_CONFIG_MODE;
  configModeStartTime = millis();
  
  // Disconnect from current WiFi
  WiFi.disconnect();
  
  // Scan networks
  scanNetworks();
  drawConfigModeScreen();
}

void drawConfigModeScreen() {
  tft.fillScreen(BLACK);
  tft.setTextColor(CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("TRYB KONFIGURACJI");
  
  // Rysowanie listy sieci (skrócona lista, bo doszedł przycisk)
  int yPos = 35;
  int maxNetworks = min(networkCount, 5); // Zmniejszamy do 5, żeby nie zasłaniać przycisków
  
  for (int i = 0; i < maxNetworks; i++) {
    tft.setTextColor(WHITE);
    tft.setCursor(10, yPos + 5);
    
    // Kłódka
    if (networkSecure[i]) tft.print("[*] ");
    else tft.print("[ ] ");
    
    // Nazwa
    String displayName = networkNames[i];
    if (displayName.length() > 25) displayName = displayName.substring(0, 25) + "...";
    tft.print(displayName);
    
    // Sygnał
    tft.setCursor(270, yPos + 5);
    tft.print(networkRSSI[i]);
    
    yPos += 25;
  }
  
  // === RYSOWANIE 4 PRZYCISKÓW NA DOLE (Y=190-230) ===
  int btnY = 190;
  int btnH = 45;
  int btnW = 75; // Szerokość przycisku
  int gap = 3;   // Odstęp
  
  // 1. ODSWIEZ (Niebieski) - X: 2
  tft.fillRect(2, btnY, btnW, btnH, BLUE);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("ODSWIEZ", 2 + btnW/2, btnY + btnH/2);
  
  // 2. LOKACJA (Zielony) - X: 80
  tft.fillRect(2 + btnW + gap, btnY, btnW, btnH, GREEN);
  tft.drawString("LOKACJA", 2 + btnW + gap + btnW/2, btnY + btnH/2);
  
  // 3. OFFLINE (Pomarańczowy) - X: 158 <--- NOWOŚĆ
  tft.fillRect(2 + 2*(btnW + gap), btnY, btnW, btnH, ORANGE);
  tft.setTextColor(BLACK); // Czarny tekst na pomarańczowym
  tft.drawString("OFFLINE", 2 + 2*(btnW + gap) + btnW/2, btnY + btnH/2);
  
  // 4. WYJSCIE (Czerwony) - X: 236
  tft.fillRect(2 + 3*(btnW + gap), btnY, btnW, btnH, RED);
  tft.setTextColor(WHITE);
  tft.drawString("WYJSCIE", 2 + 3*(btnW + gap) + btnW/2, btnY + btnH/2);
  
  // Reset ustawień tekstu
  tft.setTextDatum(TL_DATUM);
}

// Function to check if WiFi is lost (for main.cpp screen rotation pause)
bool isWiFiLost() {
  return wifiLostDetected || (currentState == STATE_SCAN_NETWORKS && backgroundReconnectActive);
}

void handleBackgroundReconnect() {
  // Wyjdź, jeśli nie powinniśmy teraz nic robić
  if (!backgroundReconnectActive || currentState != STATE_SCAN_NETWORKS) {
    return;
  }

  // --- CZĘŚĆ 1: Sprawdź, czy próba połączenia jest W TRAKCIE ---
  if (reconnectAttemptInProgress) {
    
    // 1.1: Sprawdź, czy się udało
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Background reconnect SUCCESS! Exiting scan mode.");
      backgroundReconnectActive = false;
      reconnectAttemptInProgress = false;
      currentState = STATE_CONNECTED;
      wifiLostDetected = false;
      wifiLostTime = 0;
      
      // RESETUJ timer ekranu po udanym połączeniu
      extern ScreenManager& getScreenManager();
      getScreenManager().resetScreenTimer();
      
      onWiFiConnectedTasks();
      
      // Nie rysuj connected screen - pozwól normalnym ekranom
      Serial.println("Reconnected! Resuming normal operation.");
      return;
    }
    
    // 1.2: Sprawdź, czy próba nie trwa zbyt długo (10 sekund timeout)
    if (millis() - reconnectStartTime > WIFI_CONNECTION_TIMEOUT) { 
      Serial.println("Background reconnect attempt timed out, will retry in 19s...");
      reconnectAttemptInProgress = false; // Zezwól na nową próbę
      lastReconnectAttempt = millis(); // Ustaw timer na NASTĘPNĄ próbę za 19s
      WiFi.disconnect(); // Jawnie zatrzymaj nieudaną próbę
    }
    
    // Jeśli próba nadal trwa, po prostu wyjdź i pozwól pętli działać
    return;
  }

  // --- CZĘŚĆ 2: Sprawdź, czy czas rozpocząć NOWĄ próbę połączenia ---
  // === SPRAWDŹ CZY NIE TRWA POBIERANIE OBRAZKA ===
  extern bool isImageDownloadInProgress;
  if (isImageDownloadInProgress) {
    Serial.println("⏸️ WiFi auto-reconnect SKIPPED - image download in progress");
    return; // Pomiń auto-reconnect podczas pobierania
  }
  
  if (millis() - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
    
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    
    if (savedSSID.length() > 0) {
      Serial.printf("Background auto-reconnect to: %s (scan mode)\n", savedSSID.c_str());
      
      reconnectAttemptInProgress = true; // Ustaw flagę "próbuję"
      reconnectStartTime = millis();     // Uruchom stoper dla timeoutu
      
      // Ta funkcja jest nieblokująca!
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str()); 
      
      // Żółty pasek na samym dole ekranu - FIXED for landscape 320x240
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

  // --- CZĘŚĆ 3: Aktualizuj licznik (tylko jeśli NIE próbujemy się teraz łączyć) ---
  if (!reconnectAttemptInProgress) {
    static unsigned long lastUpdateTime = 0;
    if (millis() - lastUpdateTime > WIFI_UI_UPDATE_INTERVAL) { // Aktualizuj co 5 sekund
      lastUpdateTime = millis();
      unsigned long elapsed = millis() - lastReconnectAttempt;
      int nextAttempt = (WIFI_RECONNECT_INTERVAL - elapsed) / 1000;
      
      if (nextAttempt > 0 && nextAttempt <= 19) {
        // Rysuj na tej samej pozycji X co REFRESH (240) i tej samej szerokości (75)
        // Ustaw Y tuż pod przyciskiem REFRESH (120 + 30 + 5 odstępu = 155)
        tft.fillRect(240, 155, 75, 30, BLUE); // Dopasowano X, Y, W, H
        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setCursor(250, 165); // Wyśrodkuj tekst
        tft.printf("Next: %ds", nextAttempt);
      }
    }
  }
}

void checkWiFiConnection() {
  // Check WiFi status every 2 seconds
  if (millis() - lastWiFiCheck > WIFI_STATUS_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    
    if (currentState == STATE_CONNECTED) {
      if (!isConnected && wifiWasConnected) {
        // WiFi just lost
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
        // WiFi reconnected
        wifiLostDetected = false;
        Serial.println("WiFi RECONNECTED! Canceling auto-reconnect.");
        if (!touchActive) {
           drawConnectedScreen(tft);
        }
      }
    }
    
    wifiWasConnected = isConnected;
    
    // Debug WiFi status
    if (currentState == STATE_CONNECTED) {
      Serial.printf("WiFi status: %s, RSSI: %d dBm\n", 
                   isConnected ? "CONNECTED" : "DISCONNECTED", 
                   isConnected ? WiFi.RSSI() : 0);
    }
  }
}

void handleWiFiLoss() {
  // Wyjdź, jeśli nie powinniśmy nic robić
  if (!wifiLostDetected || currentState != STATE_CONNECTED) {
    return;
  }

  unsigned long elapsed = millis() - wifiLostTime;

  // --- CZĘŚĆ 1: Sprawdź, czy próba połączenia jest W TRAKCIE ---
  if (reconnectAttemptInProgress) {
    
    // 1.1: Sprawdź, czy się udało
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Auto-reconnect SUCCESS!");
      wifiLostDetected = false;
      reconnectAttemptInProgress = false;
      drawConnectedScreen(tft); // Narysuj ekran "CONNECTED" bez banera "LOST"
      
      // RESETUJ timer ekranu po udanym połączeniu
      extern ScreenManager& getScreenManager();
      getScreenManager().resetScreenTimer();
      return;
    }
    
    // 1.2: Sprawdź, czy próba nie trwa zbyt długo (10 sekund timeout)
    if (millis() - reconnectStartTime > WIFI_CONNECTION_TIMEOUT) { 
      Serial.println("Auto-reconnect attempt timed out, will retry in 19s...");
      reconnectAttemptInProgress = false; // Zezwól na nową próbę
      lastReconnectAttempt = millis();    // Ustaw timer na NASTĘPNĄ próbę za 19s
      WiFi.disconnect();                  // Jawnie zatrzymaj nieudaną próbę
    }
    
    // Jeśli próba nadal trwa (i nie ma timeoutu), po prostu wyjdź i pozwól pętli działać
    return;
  }

  // --- CZĘŚĆ 2: Sprawdź, czy czas rozpocząć NOWĄ próbę (co 19s) ---
  // (Tylko jeśli nie minęło jeszcze 60 sekund)
  // === SPRAWDŹ CZY NIE TRWA POBIERANIE OBRAZKA ===
  extern bool isImageDownloadInProgress;
  if (isImageDownloadInProgress) {
    Serial.println("⏸️ WiFi loss reconnect SKIPPED - image download in progress");
    return; // Pomiń reconnect podczas pobierania
  }
  
  if (elapsed < WIFI_LOSS_TIMEOUT && millis() - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
    
    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");
    
    if (savedSSID.length() > 0) {
      Serial.printf("Auto-reconnect attempt to: %s (grace period)\n", savedSSID.c_str());
      
      reconnectAttemptInProgress = true; // Ustaw flagę "próbuję"
      reconnectStartTime = millis();     // Uruchom stoper dla timeoutu
      
      // Ta funkcja jest nieblokująca!
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str()); 
      
    } else {
      Serial.println("No saved WiFi credentials, resetting timer");
      lastReconnectAttempt = millis();
    }
  }

  // --- CZĘŚĆ 3: Sprawdź, czy minął 60-sekundowy "okres łaski" ---
  if (elapsed >= WIFI_LOSS_TIMEOUT) {
    Serial.printf("%d seconds elapsed. Grace period over. Starting network scan...\n", WIFI_LOSS_TIMEOUT/1000);

    // Zanim przejdziemy dalej, sprawdź ostatni raz, czy próba w toku się nie powiodła
    if (reconnectAttemptInProgress) {
       unsigned long finalWaitStart = millis();
       while(millis() - finalWaitStart < 3000 && WiFi.status() != WL_CONNECTED) { 
         delay(100); // Małe blokujące opóźnienie jest OK *tylko* w momencie przejścia
       }
       
       if (WiFi.status() == WL_CONNECTED) {
          Serial.println("FINAL auto-reconnect SUCCESS!");
          wifiLostDetected = false;
          reconnectAttemptInProgress = false;
          drawConnectedScreen(tft);
          return;
       }
    }
    
    // OSTATECZNA PORAŻKA: Przejdź do trybu skanowania
    Serial.println("All reconnect attempts failed. Starting network scan...");
    wifiLostDetected = false;
    reconnectAttemptInProgress = false;
    backgroundReconnectActive = true; // Aktywuj logikę dla ekranu skanowania
    lastReconnectAttempt = millis();  // Zresetuj timer dla logiki skanowania
    currentState = STATE_SCAN_NETWORKS;
    
    WiFi.disconnect();
      
      scanNetworks();
      drawNetworkList(tft);
      
      // Żółty pasek na samym dole ekranu - FIXED for landscape 320x240
      tft.fillRect(10, 210, 300, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setTextSize(1);
      tft.setCursor(15, 220);
      tft.println("Brak WiFi - Polacz co 19s lub wybierz siec");
      
  } else {
    // --- CZĘŚĆ 4: Aktualizuj licznik na ekranie "CONNECTED" ---
    static unsigned long lastCountdownUpdate = 0;
    if (millis() - lastCountdownUpdate > 1000) {
      lastCountdownUpdate = millis();
      if (!touchActive) {
        drawConnectedScreen(tft); 
      }
    }
  }
}

void handleTouchInput(int16_t x, int16_t y) {
  Serial.printf("HandleTouch: State=%d, X=%d, Y=%d\n", currentState, x, y);
  
  if (currentState == STATE_SCAN_NETWORKS) {
    // Visual feedback - highlight touched area
    tft.fillCircle(x, y, 5, YELLOW);
    delay(100);
    
    // Check network list (expanded area) - EXCLUDE REFRESH button area
    if (y >= 25 && y <= 240 && !(y >= 120 && y <= 150 && x >= 240)) {  // Exclude REFRESH area
      int selectedIndex = (y - 30) / 30;
      if (selectedIndex >= 0 && selectedIndex < min(networkCount, 7)) {  // Changed from 8 to 7
        selectedNetworkIndex = selectedIndex;
        currentSSID = networkNames[selectedIndex];
        Serial.printf("NETWORK SELECTED: %s (index %d)\n", currentSSID.c_str(), selectedIndex);
        
        // Visual feedback for selection
        tft.fillRect(0, 30 + selectedIndex * 30 - 2, 240, 30, GREEN);
        delay(300);
        
        if (networkSecure[selectedIndex]) {
          currentState = STATE_ENTER_PASSWORD;
          enteredPassword = "";
          drawPasswordScreen();
          Serial.println("PASSWORD SCREEN SHOWN");
        } else {
          connectToWiFi();
        }
      }
    }
    // Refresh button with visual feedback - RIGHT SIDE MIDDLE
    else if (y >= 120 && y <= 150 && x >= 240 && x <= 315) {
      Serial.println("REFRESH BUTTON PRESSED - Rescanning networks...");
      
      // Green highlight for refresh button - RIGHT SIDE MIDDLE POSITION
      tft.fillRect(240, 120, 75, 30, GREEN);
      tft.setTextColor(WHITE);
      tft.setTextSize(1);
      tft.setCursor(250, 130);
      tft.println("SCANNING");
      delay(500);
      
      // Clear selected network when refreshing
      selectedNetworkIndex = -1;
      
      // Rescan networks and update display
      scanNetworks();
      if (networkCount > 0) {
        drawNetworkList(tft);
        Serial.printf("Network list refreshed - found %d networks\n", networkCount);
      } else {
        drawStatusMessage(tft, "No networks found - Touch to retry");
        delay(2000);
        scanNetworks();
        drawNetworkList(tft);
      }
    }
  }
  
  else if (currentState == STATE_ENTER_PASSWORD) {
    handleKeyboardTouch(x, y);
  }
  
  else if (currentState == STATE_CONNECTED) {
    // Disconnect button - RESTORED ORIGINAL
    if (y >= 280 && y <= 310) {
      WiFi.disconnect();
      currentState = STATE_SCAN_NETWORKS;
      scanNetworks();
      drawNetworkList(tft);
    }
  }
  
  else if (currentState == STATE_FAILED) {
    // Any touch to retry
    currentState = STATE_SCAN_NETWORKS;
    drawNetworkList(tft);
  }
  
  else if (currentState == STATE_SELECT_LOCATION) {
    handleLocationTouch(x, y, tft);
  }
  
  else if (currentState == STATE_ENTER_COORDINATES) {
    handleCoordinatesTouch(x, y, tft);
  }
  
  else if (currentState == STATE_CONFIG_MODE) {
    Serial.printf("CONFIG MODE TOUCH: X=%d, Y=%d\n", x, y);
    
    // Lista sieci (Y: 35 - 180)
    if (y >= 35 && y <= 180) { 
      int selectedIndex = (y - 35) / 25; 
      if (selectedIndex >= 0 && selectedIndex < min(networkCount, 5)) {
        selectedNetworkIndex = selectedIndex;
        currentSSID = networkNames[selectedIndex];
        
        // Feedback
        tft.fillRect(0, 35 + selectedIndex * 25, 320, 25, GREEN);
        delay(300);
        
        if (networkSecure[selectedIndex]) {
          currentState = STATE_ENTER_PASSWORD;
          enteredPassword = "";
          drawPasswordScreen();
        } else {
          connectToWiFi();
        }
      }
    }
    
    // === OBSŁUGA DOLNYCH PRZYCISKÓW (Y > 190) ===
    else if (y >= 190 && y <= 240) {
        
        int btnW = 75; // Musi pasować do drawConfigModeScreen
        int gap = 3;
        
        // 1. REFRESH (X: 2 - 77)
        if (x >= 2 && x <= 2 + btnW) {
             Serial.println("BTN: REFRESH");
             tft.fillRect(2, 190, btnW, 45, YELLOW); // Feedback
             tft.setTextColor(BLACK);
             tft.setTextDatum(MC_DATUM);
             tft.drawString("SCAN...", 2 + btnW/2, 190 + 22);
             tft.setTextDatum(TL_DATUM);
             delay(300);
             
             selectedNetworkIndex = -1;
             scanNetworks();
             drawConfigModeScreen();
        }
        
        // 2. LOCATION (X: 80 - 155)
        else if (x >= 2 + btnW + gap && x <= 2 + 2*btnW + gap) {
             Serial.println("BTN: LOCATION");
             tft.fillRect(2 + btnW + gap, 190, btnW, 45, YELLOW);
             delay(300);
             enterLocationSelectionMode(tft);
        }
        
        // 3. OFFLINE (X: 158 - 233) <--- NOWA LOGIKA
        else if (x >= 2 + 2*(btnW + gap) && x <= 2 + 3*btnW + 2*gap) {
             Serial.println("BTN: OFFLINE MODE");
             
             // Feedback
             tft.fillRect(2 + 2*(btnW + gap), 190, btnW, 45, YELLOW);
             tft.setTextColor(BLACK);
             tft.setTextDatum(MC_DATUM);
             tft.drawString("OK!", 2 + 2*(btnW + gap) + btnW/2, 190 + 22);
             delay(500);
             
             // AKTYWACJA TRYBU OFFLINE
             isOfflineMode = true;       // Ustaw flagę globalną
             WiFi.disconnect(true);      // Rozłącz i wyłącz WiFi
             WiFi.mode(WIFI_OFF);        // Wyłącz radio
             
             currentState = STATE_CONNECTED; // Wychodzimy z configu
             
             
             // Wymuś przejście do ekranu głównego (który teraz pokaże status offline)
             tft.fillScreen(BLACK);
             // main.cpp wykryje flagę isOfflineMode i obsłuży resztę
        }
        
// 4. EXIT (X: 236 - 311)
        else if (x >= 2 + 3*(btnW + gap)) {
             Serial.println("BTN: EXIT");
             // 1. Feedback wizualny
             tft.fillRect(2 + 3*(btnW + gap), 190, btnW, 45, YELLOW);
             delay(100);

             // 2. Pobierz zapisane dane logowania
             String savedSSID = preferences.getString("ssid", "");
             String savedPass = preferences.getString("password", "");

             // 3. Jeśli mamy dane, próbujemy się połączyć PRZED zmianą stanu
             if (savedSSID.length() > 0) {
                 // Pokaż ekran łączenia zamiast błędu
                 tft.fillScreen(BLACK);
                 tft.setTextColor(WHITE);
                 tft.setTextSize(2);
                 tft.setTextDatum(MC_DATUM);
                 tft.drawString("Wznawianie...", tft.width()/2, tft.height()/2 - 20);
                 tft.setTextSize(1);
                 tft.drawString(savedSSID, tft.width()/2, tft.height()/2 + 10);
                 
                 // Rozpocznij łączenie
                 WiFi.begin(savedSSID.c_str(), savedPass.c_str());
                 
                 // Krótkie oczekiwanie (max 4 sekundy), żeby uniknąć czerwonego paska "Lost WiFi"
                 // Jeśli połączy się szybciej, pętla przerwie
                 int wait = 0;
                 while (WiFi.status() != WL_CONNECTED && wait < 8) { // 8 * 500ms = 4s
                     delay(500);
                     wait++;
                     Serial.print(".");
                 }
             }

             // 4. Powrót do normalnego stanu
             currentState = STATE_CONNECTED; 
             tft.fillScreen(BLACK);
             
             // 5. Sprawdzenie rezultatu
             if (WiFi.status() == WL_CONNECTED) {
                 // Jest OK - resetujemy flagi błędów
                 wifiLostDetected = false;
                 wifiLostTime = 0;
                 reconnectAttemptInProgress = false;
                 backgroundReconnectActive = false;
                 
                 extern ScreenManager& getScreenManager();
                 getScreenManager().resetScreenTimer();
                 
                 // Wymuś odświeżenie ekranu (np. pogody)
                 getScreenManager().forceScreenRefresh(tft);
             } else {
                 // Nadal brak połączenia - dopiero teraz uznajemy to za "Lost WiFi"
                 Serial.println("Nie udalo sie wznowic polaczenia przy wyjsciu.");
                 wifiLostDetected = true;
                 wifiLostTime = millis();
                 // Ustawiamy timer tak, by auto-reconnect spróbował ponownie za chwilę
                 lastReconnectAttempt = millis(); 
                 wifiWasConnected = true;
             }
        }
    }
  }
}

void handleKeyboardTouch(int16_t x, int16_t y) {
  int keyWidth = 25;
  int keyHeight = 28;
  int startY = 85;
  
  // Check SHOW/HIDE password button
  if (y >= 50 && y <= 75 && x >= 240 && x <= 315) {
    showPassword = !showPassword;
    Serial.printf("Password visibility toggled: %s\n", showPassword ? "POKAZ" : "UKRYJ");
    
    // Visual feedback
    tft.fillRect(240, 50, 75, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setCursor(255, 58);
    tft.print("TOGGLE");
    delay(200);
    
    drawPasswordScreen();
    return;
  }
  
  // Check main keyboard area
  if (y >= startY && y <= startY + 4 * (keyHeight + 1)) {
    int row = (y - startY) / (keyHeight + 1);
    int col = (x - 2) / (keyWidth + 1);
    
    if (row >= 0 && row < 4 && col >= 0 && col < 12) {
      String keys[4][12];
      
      if (specialMode) {
        String specialKeys[4][12] = {
          {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+"},
          {"{", "}", "|", "\\", ":", "\"", "<", ">", "?", "~", "`", "="},
          {"[", "]", ";", "'", ",", ".", "/", "-", "€", "£", "¥", "§"},
          {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "±", "×"}
        };
        for (int i = 0; i < 4; i++) {
          for (int j = 0; j < 12; j++) {
            keys[i][j] = specialKeys[i][j];
          }
        }
      } else {
        String normalKeys[4][12] = {
          {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "@", "#"},
          {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "!", "?"},
          {"a", "s", "d", "f", "g", "h", "j", "k", "l", ".", "_", "-"},
          {"z", "x", "c", "v", "b", "n", "m", ",", ";", ":", "&", "*"}
        };
        for (int i = 0; i < 4; i++) {
          for (int j = 0; j < 12; j++) {
            keys[i][j] = normalKeys[i][j];
          }
        }
      }
      
      String key = keys[row][col];
      if (key != "") {
        if (!specialMode && capsLock && key.length() == 1 && key[0] >= 'a' && key[0] <= 'z') {
          key.toUpperCase();
        }
        enteredPassword += key;
        Serial.printf("Added character: %s\n", key.c_str());
        drawPasswordScreen();
      }
    }
  }
  
  // Special keys
  else if (y >= startY + 4 * (keyHeight + 1) && y <= startY + 5 * (keyHeight + 1)) {
    if (x >= 2 && x <= 37) { // Caps Lock / ABC toggle
      if (specialMode) {
        specialMode = false;
        Serial.println("Switched to ABC mode");
      } else {
        capsLock = !capsLock;
        Serial.println("CAPS LOCK toggled");
      }
      drawPasswordScreen();
    }
    else if (x >= 40 && x <= 75) { // Special characters toggle
      specialMode = !specialMode;
      Serial.printf("Special mode: %s\n", specialMode ? "ON" : "OFF");
      drawPasswordScreen();
    }
    else if (x >= 80 && x <= 140) { // Space
      enteredPassword += " ";
      Serial.println("SPACE added");
      drawPasswordScreen();
    }
    else if (x >= 145 && x <= 180) { // Delete
      if (enteredPassword.length() > 0) {
        enteredPassword.remove(enteredPassword.length() - 1);
        Serial.println("CHARACTER deleted");
        drawPasswordScreen();
      }
    }
    else if (x >= 185 && x <= 235) { // Connect - RESTORED WORKING  
      Serial.println("CONNECT button pressed");
      connectToWiFi();
    }
    else if (x >= 240 && x <= 315) { // Back button - RESTORED WORKING
      Serial.println("BACK to network list");
      currentState = STATE_SCAN_NETWORKS;
      scanNetworks();
      drawNetworkList(tft);
    }
  }
}

// === LOCATION SELECTION IMPLEMENTATION ===

void enterLocationSelectionMode(TFT_eSPI& tft) {
  Serial.println("Entering LOCATION SELECTION MODE");
  currentState = STATE_SELECT_LOCATION;
  currentMenuState = MENU_MAIN; // Start with main menu
  currentLocationIndex = 0;
  selectedCityIndex = 0;
  drawLocationScreen(tft);
}

void drawLocationScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  
  if (currentMenuState == MENU_MAIN) {
    tft.println("WYBIERZ MIASTO");
  } else {
    tft.printf("DZIELNICE: %s", mainMenuOptions[selectedCityIndex]);
  }
  
  // Określ źródło danych
  const WeatherLocation* cityList = nullptr;
  int cityCount = 0;
  
  if (currentMenuState == MENU_MAIN) {
    // Główne menu - 4 opcje
    cityCount = 4; // Szczecin, Poznan, Zlocieniec, Wlasny GPS
  } else if (currentMenuState == MENU_DISTRICTS) {
    // Lista dzielnic dla wybranego miasta
    if (selectedCityIndex == 0) {
      cityList = SZCZECIN_DISTRICTS;
      cityCount = SZCZECIN_DISTRICTS_COUNT;
    } else if (selectedCityIndex == 1) {
      cityList = POZNAN_DISTRICTS;  
      cityCount = POZNAN_DISTRICTS_COUNT;
    } else if (selectedCityIndex == 2) {
      cityList = ZLOCIENIEC_AREAS;
      cityCount = ZLOCIENIEC_AREAS_COUNT;
    }
  }
  
  // Display current location
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.printf("Aktualna: %s", currentLoc.displayName);
  
  // Lista elementów z przewijaniem
  tft.setTextColor(WHITE);
  int yPos = 60;
  int maxVisibleItems = 5; // Maksymalnie 5 elementów na ekranie
  
  // Oblicz offset dla przewijania
  int scrollOffset = 0;
  if (currentLocationIndex >= maxVisibleItems) {
    scrollOffset = currentLocationIndex - maxVisibleItems + 1;
  }
  
  for (int i = 0; i < min(cityCount, maxVisibleItems); i++) {
    int itemIndex = i + scrollOffset;
    if (itemIndex >= cityCount) break;
    
    bool isSelected = (itemIndex == currentLocationIndex);
    uint16_t bgColor = isSelected ? BLUE : BLACK;
    
    tft.fillRect(10, yPos - 2, 300, 22, bgColor);
    tft.setTextColor(WHITE);
    tft.setCursor(15, yPos + 2);
    
    if (currentMenuState == MENU_MAIN) {
      // Wyświetlanie głównego menu miast
      if (isSelected) tft.print("→ ");
      else tft.print("  ");
      tft.printf("%s", mainMenuOptions[itemIndex]);
      
    } else if (currentMenuState == MENU_DISTRICTS) {
      // Wyświetlanie dzielnic wybranego miasta
      
      // --- FIX: POPRAWIONA LOGIKA PODŚWIETLANIA NA ZIELONO ---
      // Sprawdzamy, czy nazwa wyświetlana (Dzielnica) jest identyczna
      bool isCurrent = (String(cityList[itemIndex].displayName) == String(currentLoc.displayName));
      // -------------------------------------------------------
      
      if (isCurrent) {
        tft.fillRect(10, yPos - 2, 300, 22, GREEN);
        tft.print("● ");
      } else if (isSelected) {
        tft.print("→ ");
      } else {
        tft.print("  ");
      }
      
      tft.printf("%s", cityList[itemIndex].displayName);
      
      // Współrzędne GPS
      tft.setCursor(200, yPos + 2);
      tft.printf("%.4f,%.4f", cityList[itemIndex].latitude, cityList[itemIndex].longitude);
    }
    
    yPos += 25;
  }
  
  // Navigation buttons
  tft.fillRect(10, 210, 50, 25, DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(25, 218);
  tft.print("UP");
  
  tft.fillRect(70, 210, 50, 25, DARKGRAY);
  tft.setCursor(80, 218);
  tft.print("DOWN");
  
  // Action buttons
  tft.fillRect(130, 210, 60, 25, GREEN);
  tft.setCursor(145, 218);
  tft.print("WYBIERZ");
  
  tft.fillRect(200, 210, 50, 25, RED);
  tft.setCursor(213, 218);
  tft.print("POWROT");
  
  tft.fillRect(260, 210, 55, 25, ORANGE);
  tft.setCursor(270, 218);
  tft.print("ZAPISZ");
  
  // Wskazówka przewijania
  if (cityCount > 5) {
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setCursor(10, 185);
    if (currentMenuState == MENU_MAIN) {
      tft.printf("(4 opcje - uzyj UP/DOWN)");
    } else {
      tft.printf("(%d dzielnic - uzyj UP/DOWN)", cityCount);
    }
  }
  
  // Custom coordinates button
  tft.fillRect(10, 240, 100, 25, PURPLE);
  tft.setCursor(25, 248);
  tft.print("CUSTOM GPS");
}

void handleLocationTouch(int16_t x, int16_t y, TFT_eSPI& tft) {
  Serial.printf("Location Touch: X=%d, Y=%d\n", x, y);
  
  // Category selection (kliknięcie na listę)
  if (y >= 60 && y <= 185) {
    int clickedIndex = (y - 60) / 25;
    
    int scrollOffset = 0;
    if (currentLocationIndex >= 5) {
      scrollOffset = currentLocationIndex - 5 + 1;
    }
    
    int realIndex = clickedIndex + scrollOffset;
    
    if (currentMenuState == MENU_MAIN) {
      if (realIndex >= 0 && realIndex < 4) {
        currentLocationIndex = realIndex;
        selectedCityIndex = realIndex;
        
        if (realIndex == 3) {
          enterCoordinatesMode(tft);
          return;
        } else {
          currentMenuState = MENU_DISTRICTS;
          currentLocationIndex = 0; 
          Serial.printf("Wybrano miasto: %s\n", mainMenuOptions[selectedCityIndex]);
        }
        drawLocationScreen(tft);
        return;
      }
    } 
    else if (currentMenuState == MENU_DISTRICTS) {
       int maxItems = 0;
       if (selectedCityIndex == 0) maxItems = SZCZECIN_DISTRICTS_COUNT;
       else if (selectedCityIndex == 1) maxItems = POZNAN_DISTRICTS_COUNT;
       else if (selectedCityIndex == 2) maxItems = ZLOCIENIEC_AREAS_COUNT;

       if (realIndex >= 0 && realIndex < maxItems) {
          currentLocationIndex = realIndex;
          drawLocationScreen(tft);
          return;
       }
    }
  }
  
  // City selection shortcut (kliknięcie w nagłówek)
  if (y >= 90 && y <= 210) { // To wygląda na stary fragment logiki, ale zostawiam jak było
    int cityIndex = (y - 90) / 25;
    int maxCities = min((int)SZCZECIN_DISTRICTS_COUNT, 5);
    
    if (cityIndex >= 0 && cityIndex < maxCities) {
      currentLocationIndex = cityIndex;
      drawLocationScreen(tft);
      return;
    }
  }
  
  // Navigation buttons
  if (y >= 210 && y <= 235) {
    // UP button
    if (x >= 10 && x <= 60) {
      if (currentLocationIndex > 0) {
        currentLocationIndex--;
        drawLocationScreen(tft);
      }
    }
    // DOWN button
    else if (x >= 70 && x <= 120) {
      int maxItems = 0;
      if (currentMenuState == MENU_MAIN) maxItems = 4;
      else if (currentMenuState == MENU_DISTRICTS) {
        if (selectedCityIndex == 0) maxItems = SZCZECIN_DISTRICTS_COUNT;
        else if (selectedCityIndex == 1) maxItems = POZNAN_DISTRICTS_COUNT;
        else if (selectedCityIndex == 2) maxItems = ZLOCIENIEC_AREAS_COUNT;
      }
      
      if (currentLocationIndex < maxItems - 1) {
        currentLocationIndex++;
        drawLocationScreen(tft);
      }
    }
    // SELECT button (TUTAJ JEST KLUCZOWA ZMIANA)
    else if (x >= 130 && x <= 190) {
      if (currentMenuState == MENU_MAIN) {
        if (currentLocationIndex == 3) {
          enterCoordinatesMode(tft);
          return;
        } else {
          selectedCityIndex = currentLocationIndex;
          currentMenuState = MENU_DISTRICTS;
          currentLocationIndex = 0;
          drawLocationScreen(tft);
          return;
        }
      } else if (currentMenuState == MENU_DISTRICTS) {
        const WeatherLocation* cityList = nullptr;
        if (selectedCityIndex == 0) cityList = SZCZECIN_DISTRICTS;
        else if (selectedCityIndex == 1) cityList = POZNAN_DISTRICTS;
        else if (selectedCityIndex == 2) cityList = ZLOCIENIEC_AREAS;
        
        if (cityList) {
          WeatherLocation selectedLocation = cityList[currentLocationIndex];
          locationManager.setLocation(selectedLocation);
          isLocationSavePending = true;
          Serial.printf("Location set to: %s\n", selectedLocation.displayName);
          
          // === FIX: RESETUJEMY DANE POGODOWE ===
          // To usuwa "duchy" starego miasta
          weather.isValid = false;
          weeklyForecast.isValid = false;
          forecast.isValid = false;
          // =====================================
          
          // Visual feedback
          tft.fillRect(130, 210, 60, 25, YELLOW);
          tft.setTextColor(BLACK);
          tft.setCursor(145, 218);
          tft.print("SET!");
          delay(100);
          tft.setTextColor(WHITE);
          tft.setCursor(140, 218);
          tft.print("SAFE");
      
          // Wymuszenie odświeżenia
          extern bool weatherErrorModeGlobal;
          extern bool forecastErrorModeGlobal;
          extern bool weeklyErrorModeGlobal;
          weatherErrorModeGlobal = true; 
          forecastErrorModeGlobal = true;
          weeklyErrorModeGlobal = true;

          extern unsigned long lastWeatherCheckGlobal;
          extern unsigned long lastForecastCheckGlobal;
          lastWeatherCheckGlobal = millis() - 20000; // Użyj stałej jeśli masz zdefiniowaną
          lastForecastCheckGlobal = millis() - 20000;

          extern unsigned long lastWeeklyUpdate;
          lastWeeklyUpdate = millis() - 15000000;

          Serial.println("⏰ FORCING immediate weather refresh in main loop...");
          delay(150);
          drawLocationScreen(tft);
        }
      }
    }
    // BACK button
    else if (x >= 200 && x <= 250) {
      if (currentMenuState == MENU_DISTRICTS) {
        currentMenuState = MENU_MAIN;
        currentLocationIndex = selectedCityIndex;
        drawLocationScreen(tft);
        return;
      } else {
        currentState = STATE_CONFIG_MODE;
        drawConfigModeScreen();
        return;
      }
    }
    // SAVE button
    else if (x >= 260 && x <= 315) {
      tft.fillRect(260, 210, 55, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setCursor(270, 218);
      tft.print("ZAPISANO");
      delay(100);
      
      currentState = STATE_CONFIG_MODE;
      drawConfigModeScreen();
    }
  }
  
  // Custom GPS coordinates button
  if (y >= 240 && y <= 265 && x >= 10 && x <= 110) {
    tft.fillRect(10, 240, 100, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setCursor(25, 248);
    tft.print("OPENING");
    delay(100);
    enterCoordinatesMode(tft);
  }
}

void enterCoordinatesMode(TFT_eSPI& tft) {
  Serial.println("Entering CUSTOM COORDINATES MODE");
  currentState = STATE_ENTER_COORDINATES;
  
  // Initialize with current location coordinates
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  customLatitude = String(currentLoc.latitude, 4);
  customLongitude = String(currentLoc.longitude, 4);
  editingLatitude = true;
  coordinatesCursorPos = 0;
  
  drawCoordinatesScreen(tft);
}

void drawCoordinatesScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("WLASNE WSPOLRZEDNE");
  
  // Instructions
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.println("Wpisz wspolrzedne GPS (dok. 4 miejsca po przecinku):");
  
  // Current location
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  tft.setTextColor(GRAY);
  tft.setCursor(10, 50);
  tft.printf("Current: %.4f, %.4f", currentLoc.latitude, currentLoc.longitude);
  
  // Latitude input field
  uint16_t latColor = editingLatitude ? CYAN : WHITE;
  tft.setTextColor(latColor);
  tft.setTextSize(1);
  tft.setCursor(10, 80);
  tft.print("Szer. geogr. (N/S): ");
  
  // Latitude input box
  tft.fillRect(130, 75, 100, 20, editingLatitude ? BLUE : DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(135, 80);
  tft.print(customLatitude);
  
  // Cursor Lat
  if (editingLatitude && (millis() / 500) % 2) {
    int cursorX = 135 + coordinatesCursorPos * 6;
    tft.drawLine(cursorX, 95, cursorX + 5, 95, WHITE);
  }
  
  // Longitude input field
  uint16_t lonColor = !editingLatitude ? CYAN : WHITE;
  tft.setTextColor(lonColor);
  tft.setCursor(10, 110);
  tft.print("Dlug. geogr. (E/W): ");
  
  // Longitude input box
  tft.fillRect(130, 105, 100, 20, !editingLatitude ? BLUE : DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(135, 110);
  tft.print(customLongitude);
  
  // Cursor Lon
  if (!editingLatitude && (millis() / 500) % 2) {
    int cursorX = 135 + coordinatesCursorPos * 6;
    tft.drawLine(cursorX, 125, cursorX + 5, 125, WHITE);
  }
  
  // Switch field button
  tft.fillRect(250, 80, 60, 45, PURPLE);
  tft.setTextColor(WHITE);
  tft.setCursor(255, 95);
  tft.print(editingLatitude ? "DO DLG" : "DO SZR");
  
  // === POPRAWIONA KLAWIATURA NUMERYCZNA (LEWA STRONA) ===
  // Zaczynamy od X=5, Klawisze węższe (28px)
  int keyW = 28; 
  int keyH = 25;
  int gap = 4;   // Odstęp między klawiszami
  int startX = 5;

  // Row 1: 1 2 3
  int row1[3] = {1, 2, 3};
  for (int i = 0; i < 3; i++) {
    int x = startX + i * (keyW + gap);
    int y = 140;
    tft.fillRect(x, y, keyW, keyH, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 10, y + 8);
    tft.print(row1[i]);
  }
  
  // Row 2: 4 5 6 0
  int row2[4] = {4, 5, 6, 0};
  for (int i = 0; i < 4; i++) {
    int x = startX + i * (keyW + gap);
    int y = 170;
    tft.fillRect(x, y, keyW, keyH, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 10, y + 8);
    tft.print(row2[i]);
  }
  
  // Row 3: 7 8 9
  int row3[3] = {7, 8, 9};
  for (int i = 0; i < 3; i++) {
    int x = startX + i * (keyW + gap);
    int y = 200;
    tft.fillRect(x, y, keyW, keyH, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 10, y + 8);
    tft.print(row3[i]);
  }
  
  // Special buttons: . and -
  // Dot button (obok 9)
  int dotX = startX + 3 * (keyW + gap);
  tft.fillRect(dotX, 200, keyW, keyH, DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(dotX + 10, 208);
  tft.print(".");
  
  // Minus button (obok .)
  int minusX = startX + 4 * (keyW + gap);
  tft.fillRect(minusX, 200, keyW, keyH, DARKGRAY);
  tft.setCursor(minusX + 10, 208);
  tft.print("-");
  
  // === PRZYCISKI FUNKCYJNE (PRAWA STRONA - ODSUNIĘTE) ===
  // Zaczynamy od X=175 (bezpieczny odstęp od klawiatury która kończy się na ~160)
  int funcX = 175;
  int funcW = 65; 
  int backX = 245; // Druga kolumna przycisków
  int backW = 70;

  // Row 1 (Y=140): WYCZYSC
  tft.fillRect(backX, 140, backW, 25, RED);
  tft.setTextColor(WHITE);
  tft.setCursor(backX + 10, 148);
  tft.print("WYCZYSC");
  
  // Row 2 (Y=170): WYJSCIE | COFNIJ
  tft.fillRect(funcX, 170, funcW, 25, GRAY);
  tft.setCursor(funcX + 10, 178);
  tft.print("WYJSCIE");
  
  tft.fillRect(backX, 170, backW, 25, ORANGE);
  tft.setCursor(backX + 15, 178);
  tft.print("COFNIJ");
  
  // Row 3 (Y=200): TEST | USTAW
  tft.fillRect(funcX, 200, funcW, 25, YELLOW);
  tft.setTextColor(BLACK); // Czarny tekst na żółtym
  tft.setCursor(funcX + 20, 208);
  tft.print("TEST");
  
  tft.fillRect(backX, 200, backW, 25, GREEN);
  tft.setTextColor(WHITE);
  tft.setCursor(backX + 20, 208);
  tft.print("USTAW");
  
  // Help text
  tft.setTextColor(GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 230);
  tft.print("Format np. 53.4242");
}

void handleCoordinatesTouch(int16_t x, int16_t y, TFT_eSPI& tft) {
  Serial.printf("🎯 Coordinates Touch: X=%d, Y=%d\n", x, y);
  
  // Field selection
  if (y >= 75 && y <= 95 && x >= 130 && x <= 230) {
    editingLatitude = true;
    coordinatesCursorPos = customLatitude.length();
    drawCoordinatesScreen(tft);
    return;
  }
  
  if (y >= 105 && y <= 125 && x >= 130 && x <= 230) {
    editingLatitude = false;
    coordinatesCursorPos = customLongitude.length();
    drawCoordinatesScreen(tft);
    return;
  }
  
  // Switch button
  if (y >= 80 && y <= 125 && x >= 250 && x <= 310) {
    editingLatitude = !editingLatitude;
    String& currentField = editingLatitude ? customLatitude : customLongitude;
    coordinatesCursorPos = currentField.length();
    drawCoordinatesScreen(tft);
    return;
  }
  
  // === OBSŁUGA KLAWIATURY (NOWE WSPÓŁRZĘDNE) ===
  String& currentField = editingLatitude ? customLatitude : customLongitude;
  
  // Definicje wymiarów (muszą pasować do drawCoordinatesScreen)
  int keyW = 28; 
  int gap = 4;
  int startX = 5;
  
  // Funkcja pomocnicza do sprawdzania kolumn
  auto getCol = [&](int tx) -> int {
    if (tx < startX) return -1;
    return (tx - startX) / (keyW + gap);
  };

  // Row 1: 1 2 3 (y=140-165)
  if (y >= 140 && y <= 165) {
    int col = getCol(x);
    if (col >= 0 && col <= 2 && x <= startX + col*(keyW+gap) + keyW) {
       currentField += String(col + 1); // 0->1, 1->2, 2->3
       coordinatesCursorPos = currentField.length();
       drawCoordinatesScreen(tft);
       return;
    }
    // Clear button (Row 1 Right side)
    if (x >= 245 && x <= 315) {
       currentField = "";
       coordinatesCursorPos = 0;
       drawCoordinatesScreen(tft);
       return;
    }
  }
  
  // Row 2: 4 5 6 0 (y=170-195)
  else if (y >= 170 && y <= 195) {
    int col = getCol(x);
    if (col >= 0 && col <= 3 && x <= startX + col*(keyW+gap) + keyW) {
      if (col == 3) currentField += "0";
      else currentField += String(col + 4); // 0->4, 1->5, 2->6
      
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
      return;
    }
    // EXIT button (175 - 240)
    if (x >= 175 && x <= 240) {
       Serial.println("🚪 EXIT button pressed");
       tft.fillRect(175, 170, 65, 25, YELLOW); // Feedback
       delay(200);
       
       currentState = STATE_SELECT_LOCATION;
       currentMenuState = MENU_MAIN;
       currentLocationIndex = 0;
       tft.fillScreen(BLACK);
       drawLocationScreen(tft);
       return;
    }
    // BACKSPACE (245 - 315)
    if (x >= 245 && x <= 315) {
       if (currentField.length() > 0) {
         currentField.remove(currentField.length() - 1);
         coordinatesCursorPos = currentField.length();
         drawCoordinatesScreen(tft);
       }
       return;
    }
  }
  
  // Row 3: 7 8 9 . - (y=200-225)
  else if (y >= 200 && y <= 225) {
    int col = getCol(x);
    if (col >= 0 && col <= 4 && x <= startX + col*(keyW+gap) + keyW) {
      if (col <= 2) currentField += String(col + 7); // 7,8,9
      else if (col == 3 && currentField.indexOf('.') == -1) currentField += ".";
      else if (col == 4 && currentField.length() == 0) currentField += "-";
      
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
      return;
    }
    
    // TEST button (175 - 240)
    if (x >= 175 && x <= 240) {
       float lat = customLatitude.toFloat();
       float lon = customLongitude.toFloat();
       // Visual feedback
       tft.fillRect(175, 200, 65, 25, CYAN);
       tft.setTextColor(WHITE);
       tft.setCursor(185, 208);
       if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) tft.print("OK");
       else tft.print("ERR");
       delay(500);
       drawCoordinatesScreen(tft);
       return;
    }
    
    // SET button (245 - 315)
    if (x >= 245 && x <= 315) {
       float lat = customLatitude.toFloat();
       float lon = customLongitude.toFloat();
       
       if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
          tft.fillRect(245, 200, 70, 25, RED);
          delay(500);
          drawCoordinatesScreen(tft);
          return;
       }
       
       // Success feedback
       tft.fillRect(245, 200, 70, 25, WHITE);
       tft.setTextColor(BLACK);
       tft.setCursor(255, 208);
       tft.print("OK!");
       delay(200);
       
       // Create location
       WeatherLocation customLocation;
       customLocation.cityName = "Wlasne wsp";
       customLocation.countryCode = "XX";
       
       // Dynamiczna nazwa z numerami (formatowanie)
       static char tempDisplayName[64];
       snprintf(tempDisplayName, sizeof(tempDisplayName), "GPS %.4f,%.4f", lat, lon);
       customLocation.displayName = tempDisplayName;
       
       customLocation.latitude = lat;
       customLocation.longitude = lon;
       static char tempTimezone[32] = "UTC0";
       customLocation.timezone = tempTimezone;
       
       locationManager.setLocation(customLocation);
       isLocationSavePending = true;

       // === FIX: RESET DANYCH POGODOWYCH (DODANE) ===
       // Usuwamy stare dane, aby nie mieszały się z nowymi
       weather.isValid = false;
       weeklyForecast.isValid = false;
       forecast.isValid = false;
       // ============================================
       
       // Force refresh flags
       extern bool weatherErrorModeGlobal;
       extern bool forecastErrorModeGlobal;
       extern bool weeklyErrorModeGlobal;
       weatherErrorModeGlobal = true;
       forecastErrorModeGlobal = true;
       weeklyErrorModeGlobal = true;
       
       extern unsigned long lastWeatherCheckGlobal;
       extern unsigned long lastForecastCheckGlobal;
       lastWeatherCheckGlobal = millis() - 20000;
       lastForecastCheckGlobal = millis() - 20000;
       
       extern unsigned long lastWeeklyUpdate;
       lastWeeklyUpdate = millis() - 15000000;
       
       Serial.println("✅ Custom GPS Set & Refresh Triggered");
       
       currentState = STATE_SELECT_LOCATION;
       currentMenuState = MENU_MAIN;
       currentLocationIndex = 0;
       tft.fillScreen(BLACK);
       drawLocationScreen(tft);
    }
  }
}

// OLD TOUCH FUNCTIONS REMOVED - kod używa teraz tft.getTouch() z kalibracją
// Lepsze dla kompatybilności i używa skalibrowanej macierzy dotyku