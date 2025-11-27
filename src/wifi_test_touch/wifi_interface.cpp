#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Preferences.h>

// Hardware pins
#define TFT_BL   25  // Backlight
#define T_CS     22  // Touch CS
#define T_IRQ    34  // Touch IRQ

// Colors
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GRAY    0x8410
#define DARKGRAY 0x4208

TFT_eSPI tft = TFT_eSPI();
Preferences preferences;

// WiFi management
String defaultSSID = "YourDefaultWiFi";
String defaultPassword = "YourDefaultPassword";
String currentSSID = "";
String currentPassword = "";

// UI state
enum AppState {
  STATE_CONNECTING,
  STATE_SCAN_NETWORKS,
  STATE_ENTER_PASSWORD,
  STATE_CONNECTED,
  STATE_FAILED,
  STATE_CONFIG_MODE    // New: Configuration mode (120 sec timeout)
};

AppState currentState = STATE_CONNECTING;
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

// Network data
int networkCount = 0;
String networkNames[20];
int networkRSSI[20];
bool networkSecure[20];

// Touch functions
uint16_t readTouch(uint8_t command);
bool getTouchPoint(int16_t &x, int16_t &y);
void handleTouchInput(int16_t x, int16_t y);
void handleKeyboardTouch(int16_t x, int16_t y);

// Display functions
void drawStatusMessage(String message);
void drawConnectedScreen();
void scanNetworks();
void drawNetworkList();
void drawPasswordScreen();
void drawKeyboard();
void connectToWiFi();
void handleLongPress();
void handleConfigModeTimeout();
void enterConfigMode();
void drawConfigModeScreen();
void checkWiFiConnection();
void handleWiFiLoss();
void handleBackgroundReconnect();

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== WiFi Touch Interface ===");
  
  // Initialize preferences
  preferences.begin("wifi", false);
  
  // Initialize TFT
  tft.init();
  tft.setRotation(1);  // Landscape - as requested
  tft.fillScreen(BLACK);
  
  // Apply YOUR calibration data
  uint16_t calData[5] = { 350, 3267, 523, 3020, 1 };
  tft.setTouch(calData);
  Serial.println("Touch calibration applied to WiFi interface!");
  
  // Initialize backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  Serial.println("Hardware initialized");
  
  // Load saved credentials
  defaultSSID = preferences.getString("ssid", "YourWiFi");
  defaultPassword = preferences.getString("password", "password");
  
  drawStatusMessage("Connecting to WiFi...");
  Serial.println("Trying default WiFi...");
  
  // Try default WiFi
  WiFi.begin(defaultSSID.c_str(), defaultPassword.c_str());
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
    drawStatusMessage("Connecting... " + String(timeout) + "/20");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    currentState = STATE_CONNECTED;
    drawConnectedScreen();
    Serial.println("Connected to default WiFi!");
  } else {
    Serial.println("Default WiFi failed, scanning networks...");
    currentState = STATE_SCAN_NETWORKS;
    scanNetworks();
    drawNetworkList();
  }
}

void loop() {
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
    handleLongPress();
  } else {
    // Reset touch state when in config mode to avoid conflicts
    touchActive = false;
    longPressDetected = false;
  }
  
  // Use built-in calibrated TFT_eSPI touch function
  if (tft.getTouch(&x, &y)) {
    Serial.printf("Calibrated Touch: X=%d, Y=%d\n", x, y);
    handleTouchInput((int16_t)x, (int16_t)y);
    delay(150); // Shorter debounce for better responsiveness in config mode
  }
  delay(30);
}

void drawStatusMessage(String message) {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println(message);
}

void drawConnectedScreen() {
  tft.fillScreen(GREEN);
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
    int remaining = (60000 - elapsed) / 1000;  // Changed to 60 seconds
    if (remaining > 0) {
      tft.fillRect(10, 160, 300, 20, RED);
      tft.setTextColor(WHITE);
      tft.setCursor(15, 165);
      tft.printf("WiFi LOST! Reconnect in: %d sec (try: 37s)", remaining);
    }
  }
  
  // Disconnect button
  tft.fillRect(10, 280, 220, 30, RED);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(60, 290);
  tft.println("DISCONNECT");
}

void scanNetworks() {
  drawStatusMessage("Scanning networks...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  networkCount = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", networkCount);
  
  if (networkCount == 0) {
    drawStatusMessage("No networks found");
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

void drawNetworkList() {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("Select WiFi Network:");
  
  int yPos = 30;
  int maxNetworks = min(networkCount, 8);
  
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
  
  // Refresh button
  tft.fillRect(10, 280, 100, 30, GRAY);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.setCursor(30, 290);
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
  
  // Password field
  tft.drawRect(10, 50, 220, 25, WHITE);
  tft.fillRect(11, 51, 218, 23, BLACK);
  tft.setCursor(15, 58);
  
  // Show password with asterisks
  String displayPassword = "";
  for (int i = 0; i < enteredPassword.length(); i++) {
    displayPassword += "*";
  }
  tft.print(displayPassword);
  
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
  
  // Connect button
  tft.fillRect(185, specialY, 50, keyHeight, GREEN);
  tft.drawRect(185, specialY, 50, keyHeight, WHITE);
  tft.setCursor(195, specialY + 10);
  tft.print("CONN");
  
  // Back button - BIGGER and more visible
  tft.fillRect(240, specialY, 75, keyHeight, RED);
  tft.drawRect(240, specialY, 75, keyHeight, WHITE);
  tft.setTextColor(WHITE);
  tft.setCursor(255, specialY + 10);
  tft.print("BACK");
}

void connectToWiFi() {
  drawStatusMessage("Connecting to " + currentSSID + "...");
  Serial.println("Connecting to: " + currentSSID);
  
  currentPassword = enteredPassword;
  WiFi.begin(currentSSID.c_str(), currentPassword.c_str());
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
    drawStatusMessage("Connecting... " + String(timeout) + "/30");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Save successful credentials
    preferences.putString("ssid", currentSSID);
    preferences.putString("password", currentPassword);
    
    currentState = STATE_CONNECTED;
    drawConnectedScreen();
    Serial.println("\nConnected successfully!");
  } else {
    Serial.println("\nConnection failed!");
    tft.fillScreen(RED);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 100);
    tft.println("FAILED!");
    tft.setCursor(10, 130);
    tft.println("Touch to retry");
    
    delay(3000);
    currentState = STATE_SCAN_NETWORKS;
    drawNetworkList();
  }
}

void handleLongPress() {
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
      drawConnectedScreen(); // Clear progress bar
    }
    
    touchActive = false;
    longPressDetected = false;
    Serial.println("Touch ended");
  }
  else if (currentTouch && touchActive && !longPressDetected) {
    // Touch continues - check for long press
    if (millis() - touchStartTime >= 5000) { // 5 seconds
      longPressDetected = true;
      Serial.println("LONG PRESS DETECTED - Entering config mode!");
      
      // Only enter config mode from connected state
      if (currentState == STATE_CONNECTED) {
        enterConfigMode();
      }
    }
    else {
      // Show progress indicator for long press
      unsigned long elapsed = millis() - touchStartTime;
      if (elapsed >= 1000 && elapsed < 5000) {
        int progress = map(elapsed, 1000, 5000, 0, 100);
        
        // Draw progress bar on connected screen
        tft.fillRect(10, 10, 300, 20, BLACK);
        tft.drawRect(10, 10, 300, 20, WHITE);
        tft.fillRect(12, 12, (progress * 296) / 100, 16, YELLOW);
        
        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setCursor(130, 35);
        tft.printf("Hold for %d...", (5000 - elapsed) / 1000 + 1);
      }
    }
  }
  
  // Clear progress bar when touch is released and long press wasn't triggered
  if (!currentTouch && !longPressDetected && touchActive) {
    // Clear any progress bar remnants
    if (currentState == STATE_CONNECTED) {
      drawConnectedScreen(); // Redraw clean connected screen
    }
  }
}

void handleConfigModeTimeout() {
  if (currentState == STATE_CONFIG_MODE) {
    unsigned long elapsed = millis() - configModeStartTime;
    
    if (elapsed >= 120000) { // 120 seconds = 2 minutes
      Serial.println("Config mode timeout - returning to connected state");
      currentState = STATE_CONNECTED;
      drawConnectedScreen();
    }
    else {
      // Update countdown display
      static unsigned long lastUpdate = 0;
      if (millis() - lastUpdate > 1000) { // Update every second
        lastUpdate = millis();
        int remaining = (120000 - elapsed) / 1000;
        
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

void handleBackgroundReconnect() {
  // Only run background reconnect when in scan mode and flag is active
  if (backgroundReconnectActive && currentState == STATE_SCAN_NETWORKS) {
    
    // Try reconnecting every 37 seconds even in scan mode
    if (millis() - lastReconnectAttempt >= 37000) {
      lastReconnectAttempt = millis();
      
      String savedSSID = preferences.getString("ssid", "");
      String savedPassword = preferences.getString("password", "");
      
      if (savedSSID.length() > 0) {
        Serial.printf("Background auto-reconnect to: %s (scan mode)\n", savedSSID.c_str());
        
        // Try reconnect without disrupting current scan display
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        
        // Quick check if connection succeeds
        delay(3000);
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Background reconnect SUCCESS! Exiting scan mode.");
          backgroundReconnectActive = false;
          currentState = STATE_CONNECTED;
          drawConnectedScreen();
          return;
        } else {
          Serial.println("Background reconnect failed, staying in scan mode...");
          
          // Update the yellow message with next attempt time
          tft.fillRect(10, 200, 300, 25, YELLOW);
          tft.setTextColor(BLACK);
          tft.setTextSize(1);
          tft.setCursor(15, 210);
          tft.printf("WiFi lost - Next reconnect in 37s or select network");
        }
      }
    }
    
    // Show countdown for next attempt
    static unsigned long lastUpdateTime = 0;
    if (millis() - lastUpdateTime > 5000) { // Update every 5 seconds
      lastUpdateTime = millis();
      unsigned long elapsed = millis() - lastReconnectAttempt;
      int nextAttempt = (37000 - elapsed) / 1000;
      
      if (nextAttempt > 0 && nextAttempt <= 37) {
        tft.fillRect(250, 200, 65, 25, BLUE);
        tft.setTextColor(WHITE);
        tft.setTextSize(1);
        tft.setCursor(255, 210);
        tft.printf("Next: %ds", nextAttempt);
      }
    }
  }
}

void checkWiFiConnection() {
  // Check WiFi status every 2 seconds
  if (millis() - lastWiFiCheck > 2000) {
    lastWiFiCheck = millis();
    
    bool isConnected = (WiFi.status() == WL_CONNECTED);
    
    if (currentState == STATE_CONNECTED) {
      if (!isConnected && wifiWasConnected) {
        // WiFi just lost
        if (!wifiLostDetected) {
          wifiLostDetected = true;
          wifiLostTime = millis();
          lastReconnectAttempt = millis();
          Serial.println("WiFi LOST! Starting 60-second countdown...");
          drawConnectedScreen(); // Update display with countdown
        }
      } else if (isConnected && wifiLostDetected) {
        // WiFi reconnected
        wifiLostDetected = false;
        Serial.println("WiFi RECONNECTED! Canceling auto-reconnect.");
        drawConnectedScreen(); // Update display
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
  if (wifiLostDetected && currentState == STATE_CONNECTED) {
    unsigned long elapsed = millis() - wifiLostTime;
    
    // Update countdown every second
    static unsigned long lastCountdownUpdate = 0;
    if (millis() - lastCountdownUpdate > 1000) {
      lastCountdownUpdate = millis();
      drawConnectedScreen(); // Update countdown display
    }
    
    // Try reconnecting every 37 seconds instead of 20
    if (millis() - lastReconnectAttempt >= 37000) {
      lastReconnectAttempt = millis();
      String savedSSID = preferences.getString("ssid", "");
      String savedPassword = preferences.getString("password", "");
      
      if (savedSSID.length() > 0) {
        Serial.printf("Auto-reconnect attempt to: %s\n", savedSSID.c_str());
        WiFi.disconnect();  // Clean disconnect first
        delay(500);         // Wait for disconnect
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        
        // Wait a bit to see if connection succeeds
        delay(2000);
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Auto-reconnect SUCCESS!");
          wifiLostDetected = false;
          drawConnectedScreen();
          return; // Exit function early on success
        } else {
          Serial.println("Auto-reconnect failed, continuing countdown...");
        }
      }
    }
    
    // After 60 seconds, try one final reconnect, then go to network scan
    if (elapsed >= 60000) {
      Serial.println("60 seconds elapsed - Final reconnect attempt...");
      
      // Final reconnect attempt before giving up
      String savedSSID = preferences.getString("ssid", "");
      String savedPassword = preferences.getString("password", "");
      
      if (savedSSID.length() > 0) {
        Serial.printf("FINAL auto-reconnect attempt to: %s\n", savedSSID.c_str());
        WiFi.disconnect();
        delay(1000);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        
        // Wait longer for final attempt
        for (int i = 0; i < 10; i++) {
          delay(1000);
          if (WiFi.status() == WL_CONNECTED) {
            Serial.println("FINAL auto-reconnect SUCCESS!");
            wifiLostDetected = false;
            drawConnectedScreen();
            return;
          }
        }
      }
      
      Serial.println("All reconnect attempts failed. Starting network scan...");
      wifiLostDetected = false;
      backgroundReconnectActive = true; // Enable background reconnect in scan mode
      lastReconnectAttempt = millis();  // Reset timer for background attempts
      currentState = STATE_SCAN_NETWORKS;
      
      // Disconnect from current WiFi and scan
      WiFi.disconnect();
      delay(100);
      
      scanNetworks();
      drawNetworkList();
      
      // Show WiFi lost message at the BOTTOM to not cover first network
      tft.fillRect(10, 200, 300, 25, YELLOW);
      tft.setTextColor(BLACK);
      tft.setTextSize(1);
      tft.setCursor(15, 210);
      tft.println("WiFi lost - Reconnecting every 37s or select network");
    }
  }
}

void handleTouchInput(int16_t x, int16_t y) {
  Serial.printf("HandleTouch: State=%d, X=%d, Y=%d\n", currentState, x, y);
  
  if (currentState == STATE_SCAN_NETWORKS) {
    // Visual feedback - highlight touched area
    tft.fillCircle(x, y, 5, YELLOW);
    delay(100);
    
    // Check network list (expanded area)
    if (y >= 25 && y <= 275) {
      int selectedIndex = (y - 30) / 30;
      if (selectedIndex >= 0 && selectedIndex < min(networkCount, 8)) {
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
    // Refresh button with visual feedback
    else if (y >= 275 && y <= 315 && x >= 5 && x <= 115) {
      Serial.println("REFRESH BUTTON PRESSED");
      
      // Green highlight for refresh button
      tft.fillRect(10, 280, 100, 30, GREEN);
      tft.setTextColor(BLACK);
      tft.setTextSize(1);
      tft.setCursor(30, 290);
      tft.println("REFRESH");
      delay(300);
      
      scanNetworks();
      drawNetworkList();
    }
  }
  
  else if (currentState == STATE_ENTER_PASSWORD) {
    handleKeyboardTouch(x, y);
  }
  
  else if (currentState == STATE_CONNECTED) {
    // Disconnect button
    if (y >= 280 && y <= 310) {
      WiFi.disconnect();
      currentState = STATE_SCAN_NETWORKS;
      scanNetworks();
      drawNetworkList();
    }
  }
  
  else if (currentState == STATE_FAILED) {
    // Any touch to retry
    currentState = STATE_SCAN_NETWORKS;
    drawNetworkList();
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
    // Exit config mode button (adjusted coordinates for landscape)
    else if (y >= 200 && y <= 240 && x >= 250 && x <= 315) {
      Serial.println("EXIT CONFIG MODE - Button pressed");
      currentState = STATE_CONNECTED;
      drawConnectedScreen();
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
    else if (x >= 185 && x <= 235) { // Connect
      Serial.println("CONNECT button pressed");
      connectToWiFi();
    }
    else if (x >= 240 && x <= 315) { // Back button - BIGGER area
      Serial.println("BACK to network list");
      currentState = STATE_SCAN_NETWORKS;
      scanNetworks();
      drawNetworkList();
    }
  }
}

uint16_t readTouch(uint8_t command) {
  digitalWrite(T_CS, LOW);
  delayMicroseconds(1);
  
  SPI.transfer(command);
  delayMicroseconds(1);
  
  uint16_t result = SPI.transfer(0) << 8;
  result |= SPI.transfer(0);
  result >>= 3;
  
  delayMicroseconds(1);
  digitalWrite(T_CS, HIGH);
  delayMicroseconds(1);
  
  return result & 0x0FFF;
}

bool getTouchPoint(int16_t &x, int16_t &y) {
  uint16_t x_raw = readTouch(0x90);   // X axis  
  uint16_t y_raw = readTouch(0x91);   // Y axis
  
  // Much wider calibration range - current values X=9,Y=12 are too small
  if ((x_raw != 128 || y_raw != 128) && x_raw > 50 && y_raw > 50) {
    
    // Wider mapping to fix small coordinate issue
    x = map(x_raw, 50, 1000, 0, 240);  // Much smaller raw range
    y = map(y_raw, 50, 1000, 0, 320);  // Much smaller raw range
    
    x = constrain(x, 0, 239);
    y = constrain(y, 0, 319);
    
    Serial.printf("Raw: X=%d,Y=%d -> Mapped: X=%d,Y=%d\n", x_raw, y_raw, x, y);
    
    return true;
  }
  
  return false;
}