#include "wifi/wifi_touch_interface.h"
#include <SPI.h>
#include "managers/ScreenManager.h"
#include "sensors/motion_sensor.h"
#include "managers/MotionSensorManager.h" // Upewnij się, że masz ten include
#include "config/location_config.h"

extern bool isNtpSyncPending;
extern bool isLocationSavePending;
extern bool weatherErrorModeGlobal;  // <-- DODAJ TĘ LINIĘ
extern bool forecastErrorModeGlobal; // <-- DODAJ TĘ LINIĘ
extern bool weeklyErrorModeGlobal;

// Hardware pins - moved from header
#define TFT_BL   25  // Backlight

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

// Location selection variables
int currentLocationCategory = 0; // 0=Szczecin Districts, 1=Custom GPS
int currentLocationIndex = 0;
const char* locationCategories[] = {"Szczecin", "Custom GPS"};


// Custom coordinates variables
String customLatitude = "53.44";
String customLongitude = "14.56";
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
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
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
    drawStatusMessage(tft, "Connecting to WiFi...");
    
    // Try saved WiFi credentials
    WiFi.begin(defaultSSID.c_str(), defaultPassword.c_str());
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
      drawStatusMessage(tft, "Connecting... " + String(timeout) + "/20");
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
      tft.println("WiFi lost - Reconnecting every 19s or select network");
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
  tft.setCursor(10, 100);
  tft.println(message);
}

void drawConnectedScreen(TFT_eSPI& tft) {
  tft.fillScreen(DARK_BLUE);  // Professional dark blue for WiFi lost state
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(30, 50);
  tft.println("CONNECTED!");
  
  tft.setTextSize(1);
  tft.setCursor(10, 100);
  tft.println("Network: " + WiFi.SSID());
  tft.setCursor(10, 120);
  tft.println("IP: " + WiFi.localIP().toString());
  tft.setCursor(10, 140);
  tft.println("Signal: " + String(WiFi.RSSI()) + " dBm");
  
  // WiFi monitoring status
  if (wifiLostDetected) {
    unsigned long elapsed = millis() - wifiLostTime;
    int remaining = (WIFI_LOSS_TIMEOUT - elapsed) / 1000;
    if (remaining > 0) {
      tft.fillRect(10, 160, 300, 20, RED);
      tft.setTextColor(WHITE);
      tft.setCursor(15, 165);
      tft.printf("WiFi LOST! Reconnect in: %d sec (try: 19s)", remaining);
    }
  }
  
  // Disconnect button - RESTORED ORIGINAL
  tft.fillRect(10, 280, 220, 30, RED);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(60, 290);
  tft.println("DISCONNECT");
}

void scanNetworks() {
  drawStatusMessage(tft, "Scanning networks...");
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
  tft.println("Select WiFi Network:");
  
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
  tft.println("REFRESH");
}

void drawPasswordScreen() {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("Enter password for:");
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
    tft.print("HIDE");
  } else {
    tft.print("SHOW");
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
  tft.print("BACK");
}

void connectToWiFi() {
  drawStatusMessage(tft, "Connecting to " + currentSSID + "...");
  Serial.println("Connecting to: " + currentSSID);
  
  currentPassword = enteredPassword;
  WiFi.begin(currentSSID.c_str(), currentPassword.c_str());
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
    drawStatusMessage(tft, "Connecting... " + String(timeout) + "/30");
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
    tft.println("FAILED!");
    tft.setCursor(10, 130);
    tft.println("Touch to retry");
    
    delay(DELAY_SUCCESS_DISPLAY);
    currentState = STATE_SCAN_NETWORKS;
    drawNetworkList(tft);
  }
}

void handleLongPress(TFT_eSPI& tft) {
  uint16_t x, y;
  bool currentTouch = tft.getTouch(&x, &y);
  
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
        tft.printf("Hold for %d...", (WIFI_LONG_PRESS_TIME - elapsed) / 1000 + 1);
      }
    }
  }
  
  // Clear progress bar when touch is released and long press wasn't triggered
  if (!currentTouch && !longPressDetected && touchActive) {
    // Clear any progress bar remnants
    if (currentState == STATE_CONNECTED) {
      drawConnectedScreen(tft); // Redraw clean connected screen
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
  tft.println("CONFIG MODE");
  
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(150, 10);
  tft.println("(120 sec timeout)");
  
  // Draw refresh button
  tft.fillRect(10, 200, 65, 40, BLUE);
  tft.setTextColor(WHITE);
  tft.setCursor(20, 215);
  tft.println("REFRESH");
  
  // Draw location button
  tft.fillRect(85, 200, 75, 40, GREEN);
  tft.setTextColor(WHITE);
  tft.setCursor(95, 215);
  tft.println("LOCATION");
  
  // Draw exit button
  tft.fillRect(250, 200, 65, 40, RED);
  tft.setTextColor(WHITE);
  tft.setCursor(265, 215);
  tft.println("EXIT");
  
  // Draw network list
  int yPos = 35;
  int maxNetworks = min(networkCount, 6); // Fewer networks due to header
  
  for (int i = 0; i < maxNetworks; i++) {
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
    if (displayName.length() > 30) {
      displayName = displayName.substring(0, 30) + "...";
    }
    tft.print(displayName);
    
    // Signal strength
    tft.setCursor(270, yPos + 5);
    tft.print(networkRSSI[i]);
    
    yPos += 25;
  }
  
  Serial.printf("Config mode screen drawn with %d networks\n", maxNetworks);
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
          drawConnectedScreen(tft); // Update display with countdown
        }
      } else if (isConnected && wifiLostDetected) {
        // WiFi reconnected
        wifiLostDetected = false;
        Serial.println("WiFi RECONNECTED! Canceling auto-reconnect.");
        drawConnectedScreen(tft); // Update display
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
  
  if (elapsed < WIFI_LOSS_TIMEOUT && millis() - lastReconnectAttempt >= 19000) {
    
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
      tft.println("WiFi lost - Reconnecting every 19s or select network");
      
  } else {
    // --- CZĘŚĆ 4: Aktualizuj licznik na ekranie "CONNECTED" ---
    static unsigned long lastCountdownUpdate = 0;
    if (millis() - lastCountdownUpdate > 1000) {
      lastCountdownUpdate = millis();
      drawConnectedScreen(tft); // Odśwież ekran, aby pokazać licznik
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
    
    // Check network list (adjusted for landscape mode)
    if (y >= 35 && y <= 185) { // Network list area
      int selectedIndex = (y - 35) / 25; // 25px per network in config mode
      if (selectedIndex >= 0 && selectedIndex < min(networkCount, 6)) {
        selectedNetworkIndex = selectedIndex;
        currentSSID = networkNames[selectedIndex];
        Serial.printf("CONFIG MODE - NETWORK SELECTED: %s (index %d)\n", currentSSID.c_str(), selectedIndex);
        
        // Visual feedback for selection
        tft.fillRect(0, 35 + selectedIndex * 25, 320, 25, GREEN);
        delay(500);
        
        if (networkSecure[selectedIndex]) {
          currentState = STATE_ENTER_PASSWORD;
          enteredPassword = "";
          drawPasswordScreen();
          Serial.println("CONFIG MODE - PASSWORD SCREEN SHOWN");
        } else {
          connectToWiFi();
        }
      }
    }
    // Refresh button in config mode
    else if (y >= 200 && y <= 240 && x >= 10 && x <= 75) {
      Serial.println("CONFIG MODE - REFRESH BUTTON PRESSED");
      
      // Visual feedback
      tft.fillRect(10, 200, 65, 40, YELLOW);
      tft.setTextColor(WHITE);
      tft.setCursor(20, 215);
      tft.println("SCANNING");
      delay(500);
      
      // Clear selected network and rescan
      selectedNetworkIndex = -1;
      scanNetworks();
      drawConfigModeScreen();
      
      Serial.printf("CONFIG MODE - Network list refreshed - found %d networks\n", networkCount);
    }
    // Location selection button
    else if (y >= 200 && y <= 240 && x >= 85 && x <= 160) {
      Serial.println("CONFIG MODE - LOCATION BUTTON PRESSED");
      
      // Visual feedback
      tft.fillRect(85, 200, 75, 40, YELLOW);
      tft.setTextColor(WHITE);
      tft.setCursor(95, 215);
      tft.println("OPENING");
      delay(500);
      
      // Enter location selection mode
      enterLocationSelectionMode(tft);
    }
    // Exit config mode button (adjusted coordinates for landscape)
    else if (y >= 200 && y <= 240 && x >= 250 && x <= 315) {
      Serial.println("EXIT CONFIG MODE - Button pressed");
      currentState = STATE_CONNECTED; // (1) Wyjdź z menu
      
      // (2) Wyczyść ekran, aby pokazać, że coś się dzieje
      tft.fillScreen(COLOR_BACKGROUND);
      tft.setTextColor(TFT_YELLOW, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Ponowne laczenie...", tft.width() / 2, tft.height() / 2);
      
      // (3) Aktywuj logikę auto-reconnect i OMIŃ timer
      wifiLostDetected = true;
      wifiLostTime = millis();
      // Ustaw timer tak, jakby właśnie minęło 19 sekund
      lastReconnectAttempt = millis() - 19000; // <-- POPRAWKA: Wymuś natychmiastowe działanie
      wifiWasConnected = true;
    }
    // Debug: show all touch attempts in config mode
    else {
      Serial.printf("CONFIG MODE - Touch outside active areas: X=%d, Y=%d\n", x, y);
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
    Serial.printf("Password visibility toggled: %s\n", showPassword ? "SHOW" : "HIDE");
    
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
  currentLocationCategory = 0; // Start with Poland
  currentLocationIndex = 0;
  drawLocationScreen(tft);
}

void drawLocationScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("SELECT LOCATION");
  
  // Category buttons
  for (int i = 0; i < 2; i++) {
    uint16_t color = (i == currentLocationCategory) ? CYAN : DARKGRAY;
    tft.fillRect(10 + i * 150, 35, 140, 25, color);
    tft.setTextColor(WHITE);
    tft.setTextSize(1);
    tft.setCursor(15 + i * 150, 43);
    tft.println(locationCategories[i]);
  }
  
  // Get current city list based on category
  const WeatherLocation* cityList;
  int cityCount;
  
  if (currentLocationCategory == 0) {
    cityList = SZCZECIN_DISTRICTS;
    cityCount = SZCZECIN_DISTRICTS_COUNT;
  } else {
    // Category 1 = Custom GPS - redirect to coordinates mode
    if (currentLocationCategory == 1) {
      enterCoordinatesMode(tft);
      return;
    }
    cityList = SZCZECIN_DISTRICTS;
    cityCount = SZCZECIN_DISTRICTS_COUNT;
  }
  
  // Display current location
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 70);
  tft.printf("Current: %s", currentLoc.displayName);
  
  // City list
  tft.setTextColor(WHITE);
  int yPos = 90;
  int maxCities = min(cityCount, 5); // Show max 5 cities
  
  for (int i = 0; i < maxCities; i++) {
    bool isSelected = (i == currentLocationIndex);
    bool isCurrent = (strcmp(cityList[i].cityName, currentLoc.cityName) == 0 && 
                     strcmp(cityList[i].countryCode, currentLoc.countryCode) == 0);
    
    uint16_t bgColor = BLACK;
    if (isSelected) bgColor = BLUE;
    else if (isCurrent) bgColor = GREEN;
    
    tft.fillRect(10, yPos - 2, 300, 22, bgColor);
    
    tft.setTextColor(WHITE);
    tft.setCursor(15, yPos + 2);
    
    if (isCurrent) tft.print("● ");
    else if (isSelected) tft.print("→ ");
    else tft.print("  ");
    
    tft.printf("%s", cityList[i].displayName);
    
    // Coordinates (rounded to 2 decimal places)
    tft.setCursor(200, yPos + 2);
    tft.printf("%.2f,%.2f", cityList[i].latitude, cityList[i].longitude);
    
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
  tft.print("SELECT");
  
  tft.fillRect(200, 210, 50, 25, RED);
  tft.setCursor(215, 218);
  tft.print("BACK");
  
  tft.fillRect(260, 210, 55, 25, ORANGE);
  tft.setCursor(270, 218);
  tft.print("SAVE");
  
  // Custom coordinates button
  tft.fillRect(10, 240, 100, 25, PURPLE);
  tft.setCursor(25, 248);
  tft.print("CUSTOM GPS");
}

void handleLocationTouch(int16_t x, int16_t y, TFT_eSPI& tft) {
  Serial.printf("Location Touch: X=%d, Y=%d\n", x, y);
  
  // Category selection
  if (y >= 35 && y <= 60) {
    int newCategory = (x - 10) / 150;
    if (newCategory >= 0 && newCategory < 2 && newCategory != currentLocationCategory) {
      currentLocationCategory = newCategory;
      currentLocationIndex = 0; // Reset city index
      Serial.printf("Category changed to: %s\n", locationCategories[currentLocationCategory]);
      
      if (newCategory == 1) {
        // Direct jump to Custom GPS mode
        enterCoordinatesMode(tft);
        return;
      }
      
      drawLocationScreen(tft);
      return;
    }
  }
  
  // City selection
  if (y >= 90 && y <= 210) {
    int cityIndex = (y - 90) / 25;
    int maxCities = min((int)SZCZECIN_DISTRICTS_COUNT, 5);
    
    if (cityIndex >= 0 && cityIndex < maxCities) {
      currentLocationIndex = cityIndex;
      Serial.printf("City selected: %d\n", cityIndex);
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
      int maxCities = SZCZECIN_DISTRICTS_COUNT;
      
      if (currentLocationIndex < min(maxCities - 1, 4)) {
        currentLocationIndex++;
        drawLocationScreen(tft);
      }
    }
    // SELECT button
    else if (x >= 130 && x <= 190) {
      const WeatherLocation* cityList = SZCZECIN_DISTRICTS;
      
      WeatherLocation selectedLocation = cityList[currentLocationIndex];
      locationManager.setLocation(selectedLocation);
      isLocationSavePending = true;
      Serial.printf("Location set to: %s\n", selectedLocation.displayName);
      
      // Visual feedback
      tft.fillRect(130, 210, 60, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setCursor(145, 218);
      tft.print("SET!");
      delay(100);
      
      // SAFE WEATHER REFRESH for districts - Prevents WiFi disconnect  
      tft.fillRect(130, 210, 60, 25, PURPLE);
      tft.setTextColor(WHITE);
      tft.setCursor(140, 218);
      tft.print("SAFE");
      
      // SAFE: Delayed refresh instead of immediate (prevents WiFi crash)
      extern unsigned long lastWeatherCheckGlobal;
      extern unsigned long lastForecastCheckGlobal;

      // (1) Włącz tryb błędu, aby wymusić szybki interwał (5s)
      weatherErrorModeGlobal = true; 
      forecastErrorModeGlobal = true;
      weeklyErrorModeGlobal = true;

      // Ustawiamy timery tak, jakby właśnie wygasły (używając wartości z configu, np. 20000)
      lastWeatherCheckGlobal = millis() - 20000; 
      lastForecastCheckGlobal = millis() - 20000;

      Serial.println("⏰ FORCING immediate weather refresh in main loop...");
      
      delay(150);
      
     drawLocationScreen(tft);
    }
    // BACK button
    else if (x >= 200 && x <= 250) {
      Serial.println("Returning to CONFIG MODE");
      currentState = STATE_CONFIG_MODE;
      drawConfigModeScreen();
    }
    // SAVE button
    else if (x >= 260 && x <= 315) {
      // Location save will be executed when exiting WiFi config mode
      
      // Visual feedback
      tft.fillRect(260, 210, 55, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setCursor(270, 218);
      tft.print("SAVED");
      delay(100);
      
      // Return to config mode
      currentState = STATE_CONFIG_MODE;
      drawConfigModeScreen();
    }
  }
  
  // Custom GPS coordinates button
  if (y >= 240 && y <= 265 && x >= 10 && x <= 110) {
    Serial.println("CUSTOM GPS button pressed");
    
    // Visual feedback
    tft.fillRect(10, 240, 100, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setCursor(25, 248);
    tft.print("OPENING");
    delay(100);
    
    enterCoordinatesMode(tft);
  }
}

// === CUSTOM COORDINATES IMPLEMENTATION ===

void enterCoordinatesMode(TFT_eSPI& tft) {
  Serial.println("Entering CUSTOM COORDINATES MODE");
  currentState = STATE_ENTER_COORDINATES;
  
  // Initialize with current location coordinates
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  customLatitude = String(currentLoc.latitude, 2);
  customLongitude = String(currentLoc.longitude, 2);
  editingLatitude = true;
  coordinatesCursorPos = 0;
  
  drawCoordinatesScreen(tft);
}

void drawCoordinatesScreen(TFT_eSPI& tft) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.println("CUSTOM COORDINATES");
  
  // Instructions
  tft.setTextColor(YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.println("Enter GPS coordinates (decimal degrees):");
  
  // Current location for reference
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  tft.setTextColor(GRAY);
  tft.setCursor(10, 50);
  tft.printf("Current: %s (%.2f, %.2f)", currentLoc.displayName, currentLoc.latitude, currentLoc.longitude);
  
  // Latitude input field
  uint16_t latColor = editingLatitude ? CYAN : WHITE;
  tft.setTextColor(latColor);
  tft.setTextSize(1);
  tft.setCursor(10, 80);
  tft.print("Latitude  (N/S): ");
  
  // Latitude input box
  tft.fillRect(130, 75, 100, 20, editingLatitude ? BLUE : DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(135, 80);
  tft.print(customLatitude);
  
  // Show cursor for latitude
  if (editingLatitude && (millis() / 500) % 2) {
    int cursorX = 135 + coordinatesCursorPos * 6;
    tft.drawLine(cursorX, 95, cursorX + 5, 95, WHITE);
  }
  
  // Longitude input field
  uint16_t lonColor = !editingLatitude ? CYAN : WHITE;
  tft.setTextColor(lonColor);
  tft.setCursor(10, 110);
  tft.print("Longitude (E/W): ");
  
  // Longitude input box
  tft.fillRect(130, 105, 100, 20, !editingLatitude ? BLUE : DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(135, 110);
  tft.print(customLongitude);
  
  // Show cursor for longitude
  if (!editingLatitude && (millis() / 500) % 2) {
    int cursorX = 135 + coordinatesCursorPos * 6;
    tft.drawLine(cursorX, 125, cursorX + 5, 125, WHITE);
  }
  
  // Switch field button
  tft.fillRect(250, 80, 60, 45, PURPLE);
  tft.setTextColor(WHITE);
  tft.setCursor(260, 90);
  tft.print("SWITCH");
  tft.setCursor(265, 105);
  tft.print(editingLatitude ? "TO LON" : "TO LAT");
  
  // Numeric keypad (better layout)
  // Row 1: 1 2 3
  int row1[3] = {1, 2, 3};
  for (int i = 0; i < 3; i++) {
    int x = 10 + i * 35;
    int y = 140;
    tft.fillRect(x, y, 30, 25, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 12, y + 8);
    tft.print(row1[i]);
  }
  
  // Row 2: 4 5 6 0
  int row2[4] = {4, 5, 6, 0};
  for (int i = 0; i < 4; i++) {
    int x = 10 + i * 35;
    int y = 170;
    tft.fillRect(x, y, 30, 25, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 12, y + 8);
    tft.print(row2[i]);
  }
  
  // Row 3: 7 8 9
  int row3[3] = {7, 8, 9};
  for (int i = 0; i < 3; i++) {
    int x = 10 + i * 35;
    int y = 200;
    tft.fillRect(x, y, 30, 25, DARKGRAY);
    tft.setTextColor(WHITE);
    tft.setCursor(x + 12, y + 8);
    tft.print(row3[i]);
  }
  
  // Special buttons: . and -
  // Dot button
  tft.fillRect(115, 200, 30, 25, DARKGRAY);
  tft.setTextColor(WHITE);
  tft.setCursor(125, 208);
  tft.print(".");
  
  // Minus button  
  tft.fillRect(150, 200, 30, 25, DARKGRAY);
  tft.setCursor(160, 208);
  tft.print("-");
  
  // Right side control buttons (moved away from numbers)
  // Clear button
  tft.fillRect(220, 140, 90, 25, RED);
  tft.setTextColor(WHITE);
  tft.setCursor(245, 148);
  tft.print("CLEAR");
  
  // Backspace button
  tft.fillRect(220, 170, 90, 25, ORANGE);
  tft.setCursor(240, 178);
  tft.print("BACK<-");
  
  // SET GPS button (main action)
  tft.fillRect(220, 200, 90, 25, GREEN);
  tft.setCursor(235, 208);
  tft.print("SET GPS");
  
  // BACK button (return to location)
  tft.fillRect(160, 170, 55, 25, GRAY);
  tft.setCursor(175, 178);
  tft.print("EXIT");
  
  // TEST button (validate coordinates)
  tft.fillRect(160, 200, 55, 25, YELLOW);
  tft.setCursor(175, 208);
  tft.print("TEST");
  
  // Help text (bottom)
  tft.setTextColor(GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 230);
  tft.print("Range: Lat ±90, Lon ±180");
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
  
  // Numeric keypad - new layout
  String& currentField = editingLatitude ? customLatitude : customLongitude;
  
  // Row 1: 1 2 3 (y=140-165)
  if (y >= 140 && y <= 165) {
    if (x >= 10 && x <= 40) {
      currentField += "1";
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    } else if (x >= 45 && x <= 75) {
      currentField += "2";
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    } else if (x >= 80 && x <= 110) {
      currentField += "3";
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    }
  }
  
  // Row 2: 4 5 6 0 (y=170-195)
  else if (y >= 170 && y <= 195) {
    if (x >= 10 && x <= 40) {
      currentField += "4";
    } else if (x >= 45 && x <= 75) {
      currentField += "5";
    } else if (x >= 80 && x <= 110) {
      currentField += "6";
    } else if (x >= 115 && x <= 145) {
      currentField += "0"; // Zero on the right
    }
    coordinatesCursorPos = currentField.length();
    drawCoordinatesScreen(tft);
  }
  
  // Row 3: 7 8 9 . - (y=200-225)
  else if (y >= 200 && y <= 225) {
    if (x >= 10 && x <= 40) {
      currentField += "7";
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    } else if (x >= 45 && x <= 75) {
      currentField += "8";
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    } else if (x >= 80 && x <= 110) {
      currentField += "9";
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    } else if (x >= 115 && x <= 145 && currentField.indexOf('.') == -1) {
      currentField += "."; // Dot - only if not already present
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    } else if (x >= 150 && x <= 180 && currentField.length() == 0) {
      currentField += "-"; // Minus - only at start
      coordinatesCursorPos = currentField.length();
      drawCoordinatesScreen(tft);
    }
  }
  
  // Clear button (right side)
  if (y >= 140 && y <= 165 && x >= 220 && x <= 310) {
    String& currentField = editingLatitude ? customLatitude : customLongitude;
    currentField = "";
    coordinatesCursorPos = 0;
    drawCoordinatesScreen(tft);
    return;
  }
  
  // Backspace button (right side)
  if (y >= 170 && y <= 195 && x >= 220 && x <= 310) {
    String& currentField = editingLatitude ? customLatitude : customLongitude;
    if (currentField.length() > 0) {
      currentField.remove(currentField.length() - 1);
      coordinatesCursorPos = currentField.length();
    }
    drawCoordinatesScreen(tft);
    return;
  }
  
  // EXIT button (middle right)
  if (y >= 170 && y <= 195 && x >= 160 && x <= 215) {
    Serial.println("🚪 EXIT button pressed in Custom GPS");
    
    // Visual feedback
    tft.fillRect(160, 170, 55, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setCursor(175, 178);
    tft.print("EXITING");
    delay(500);
    
    // Proper state transition
    currentState = STATE_SELECT_LOCATION;
    currentLocationCategory = 0;  // Reset to Szczecin districts
    currentLocationIndex = 0;      // Reset selection
    
    // Clear screen and return
    tft.fillScreen(BLACK);
    drawLocationScreen(tft);
    
    Serial.println("✅ Exited to Location Selection");
    return;
  }
  
  // TEST button (middle right)  
  if (y >= 200 && y <= 225 && x >= 160 && x <= 215) {
    float lat = customLatitude.toFloat();
    float lon = customLongitude.toFloat();
    
    // Visual feedback
    tft.fillRect(160, 200, 55, 25, CYAN);
    tft.setTextColor(WHITE);
    tft.setCursor(175, 208);
    
    if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
      tft.print("VALID");
    } else {
      tft.print("ERROR");
    }
    delay(1000);
    drawCoordinatesScreen(tft);
    return;
  }
  
  // SET GPS button (main action, right side)
  if (y >= 200 && y <= 225 && x >= 220 && x <= 310) {
    Serial.println("🟢 SET GPS BUTTON PRESSED!");
    Serial.printf("📍 Touch area: Y=%d (200-225), X=%d (220-310)\n", y, x);
    Serial.printf("📊 Current values: Lat='%s', Lon='%s'\n", customLatitude.c_str(), customLongitude.c_str());
    
    // IMMEDIATE VISUAL FEEDBACK - Button pressed effect
    tft.fillRect(220, 200, 90, 25, YELLOW);
    tft.setTextColor(BLACK);
    tft.setCursor(235, 208);
    tft.print("PRESSED");
    delay(100);
    
    // SET GPS button
    if (true) {
      float lat = customLatitude.toFloat();
      float lon = customLongitude.toFloat();
      
      Serial.printf("🧮 Parsed coordinates: Lat=%.6f, Lon=%.6f\n", lat, lon);
      
      // Validate coordinates
      if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
        Serial.println("❌ COORDINATES INVALID!");
        Serial.printf("❌ Lat=%.6f (must be ±90), Lon=%.6f (must be ±180)\n", lat, lon);
        
        // Show error
        tft.fillRect(220, 200, 90, 25, RED);
        tft.setTextColor(WHITE);
        tft.setCursor(235, 208);
        tft.print("INVALID");
        delay(1500);
        drawCoordinatesScreen(tft);
        return;
      }
      
      Serial.printf("✅ Coordinates VALID: Lat=%.6f, Lon=%.6f\n", lat, lon);
      
      // Show processing feedback
      tft.fillRect(220, 200, 90, 25, CYAN);
      tft.setTextColor(WHITE);
      tft.setCursor(225, 208);
      tft.print("CREATING");
      delay(100);
      
      // Create custom location - SAFE MEMORY ALLOCATION
      WeatherLocation customLocation;
      customLocation.cityName = "Custom";
      customLocation.countryCode = "XX";
      
      // SAFE: Use static buffer instead of direct access
      static char tempDisplayName[64];  // Safe buffer
      snprintf(tempDisplayName, sizeof(tempDisplayName), "GPS %.2f,%.2f", lat, lon);
      customLocation.displayName = tempDisplayName;
      
      customLocation.latitude = lat;
      customLocation.longitude = lon;
      
      // SAFE: Use static buffer for timezone  
      static char tempTimezone[32] = "UTC0";
      customLocation.timezone = tempTimezone;
      
      locationManager.setLocation(customLocation);
      isLocationSavePending = true;
      
      // NVRAM SAVE COMPLETELY DISABLED - Prevents WiFi corruption
      // Location save will be executed when exiting WiFi config mode
      Serial.println("⚠️ NVRAM save SCHEDULED for main loop");
      
      // Show success feedback
      tft.fillRect(220, 200, 90, 25, GREEN);
      tft.setTextColor(WHITE);
      tft.setCursor(240, 208);
      tft.print("SET!");
      delay(100);
      
      // SAFE WEATHER REFRESH - Prevents WiFi disconnect
      tft.fillRect(220, 200, 90, 25, PURPLE);
      tft.setTextColor(WHITE);
      tft.setCursor(225, 208);
      tft.print("SAFE");
      
      // SAFE: Delayed refresh instead of immediate (prevents WiFi crash)
      extern unsigned long lastWeatherCheckGlobal;
      extern unsigned long lastForecastCheckGlobal;
      // Ustawiamy timery tak, jakby właśnie wygasły
      lastWeatherCheckGlobal = millis() - 20000; 
      lastForecastCheckGlobal = millis() - 20000;

      Serial.println("⏰ FORCING immediate weather refresh in main loop...");
      
      delay(150);
      
      // Final confirmation
      tft.fillRect(220, 200, 90, 25, WHITE);
      tft.setTextColor(BLACK);
      tft.setCursor(240, 208);
      tft.print("DONE");
      delay(100);
      
      // Ensure proper state transition
      Serial.println("🔙 Returning to Location Selection...");
      currentState = STATE_SELECT_LOCATION;
      currentLocationCategory = 0;  // Reset to Szczecin districts
      currentLocationIndex = 0;      // Reset selection
      
      // Clear screen before drawing location screen
      tft.fillScreen(BLACK);
      drawLocationScreen(tft);
      
      Serial.println("✅ Successfully returned to Location Selection");
    }
  }
}

// OLD TOUCH FUNCTIONS REMOVED - kod używa teraz tft.getTouch() z kalibracją
// Lepsze dla kompatybilności i używa skalibrowanej macierzy dotyku