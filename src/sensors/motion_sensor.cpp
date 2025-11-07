#include "sensors/motion_sensor.h"
#include "config/display_config.h"
#include <esp_sleep.h>

// --- ZMIENNE GLOBALNE ---
volatile bool motionDetected = false;
DisplayState currentDisplayState = DISPLAY_SLEEPING;
unsigned long lastMotionTime = 0;
unsigned long lastDisplayUpdate = 0;

void initMotionSensor() {
  Serial.println("=== INICJALIZACJA PIR MOD-01655 ===");
  
  // Konfiguruj pin PIR jako input z pull-down
  pinMode(PIR_PIN, INPUT);
  
  // Attach interrupt dla motion detection
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), motionInterrupt, RISING);
  
  // Sprawdź czy to cold start czy wake up z deep sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Wake up z PIR - display aktywny
    currentDisplayState = DISPLAY_ACTIVE;
    lastMotionTime = millis();
    Serial.println("🔥 PIR WAKE UP - Display AKTYWNY");
  } else {
    // Cold start - pokaż demo przez 30 sekund, potem sleep
    currentDisplayState = DISPLAY_ACTIVE;
    lastMotionTime = millis();
    Serial.println("🚀 COLD START - Demo 30s, potem sleep mode");
  }
  
  Serial.println("✅ PIR Sensor na GPIO " + String(PIR_PIN) + " gotowy!");
  Serial.println("🕐 Timeout: " + String(MOTION_TIMEOUT/1000) + " sekund");
}

void IRAM_ATTR motionInterrupt() {
  static unsigned long lastInterrupt = 0;
  unsigned long currentTime = millis();
  
  // Debounce - ignoruj zbyt częste przerwania
  if (currentTime - lastInterrupt > DEBOUNCE_TIME) {
    motionDetected = true;
    lastInterrupt = currentTime;
  }
}

bool isMotionActive() {
  // Sprawdź czy był ruch w ostatnim MOTION_TIMEOUT
  return (millis() - lastMotionTime) < MOTION_TIMEOUT;
}

void updateDisplayPowerState(TFT_eSPI& tft) {
  // Obsługa motion detection
  if (motionDetected) {
    motionDetected = false; // Reset flag
    lastMotionTime = millis();
    
    if (currentDisplayState == DISPLAY_SLEEPING) {
      Serial.println("🔥 MOTION DETECTED - WAKE UP DISPLAY!");
      wakeUpDisplay(tft);
    } else {
      Serial.println("🔄 Motion detected - przedłużam aktywność");
    }
  }
  
  // Sprawdź timeout
  if (currentDisplayState == DISPLAY_ACTIVE) {
    if (!isMotionActive()) {
      Serial.println("💤 Brak ruchu przez " + String(MOTION_TIMEOUT/1000) + "s - SLEEP MODE");
      sleepDisplay(tft);
    }
  }
}

void wakeUpDisplay(TFT_eSPI& tft) {
  currentDisplayState = DISPLAY_ACTIVE;
  
  // Włącz backlight (jeśli jest sterowany)
  // tft.writecommand(TFT_DISPON); // Uncomment if needed
  
  // Wyczyść ekran i pokaż info o wake up
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(TFT_GREEN, COLOR_BACKGROUND);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("WAKE UP!", tft.width() / 2, tft.height() / 2 - 20);
  tft.setTextSize(1);
  tft.drawString("Motion detected", tft.width() / 2, tft.height() / 2 + 10);
  tft.drawString("Starting weather station...", tft.width() / 2, tft.height() / 2 + 30);
  
  delay(2000); // 2 sekundy na pokazanie wake up message
  
  Serial.println("✅ Display ACTIVE - uruchamiam stację pogodową");
}

void sleepDisplay(TFT_eSPI& tft) {
  currentDisplayState = DISPLAY_SLEEPING;
  
  // Pokaż sleep message
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("SLEEP MODE", tft.width() / 2, tft.height() / 2 - 20);
  tft.setTextSize(1);
  tft.drawString("Waiting for motion...", tft.width() / 2, tft.height() / 2 + 10);
  tft.drawString("PIR MOD-01655 active", tft.width() / 2, tft.height() / 2 + 30);
  tft.drawString("Deep sleep in 3s...", tft.width() / 2, tft.height() / 2 + 50);
  
  delay(3000);
  
  // Wyłącz ekran całkowicie
  tft.fillScreen(TFT_BLACK);
  
  // Wyłącz backlight (jeśli jest sterowany)
  // tft.writecommand(TFT_DISPOFF); // Uncomment if needed
  
  Serial.println("💤 ENTERING DEEP SLEEP - PIR wake up na GPIO " + String(PIR_PIN));
  Serial.flush(); // Upewnij się że komunikat zostanie wysłany
  
  // Konfiguruj PIR jako źródło wake up z deep sleep
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1); // Wake up when PIR goes HIGH
  
  // Wejście w deep sleep - ESP32 zatrzymuje się całkowicie
  esp_deep_sleep_start();
}

DisplayState getDisplayState() {
  return currentDisplayState;
}