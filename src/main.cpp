#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// --- KONFIGURACJA ---
#include "config/wifi_config.h"
#include "config/weather_config.h"
#include "config/display_config.h"

// --- DANE I API ---
#include "weather/weather_data.h"
#include "weather/weather_api.h"

// --- WYŚWIETLANIE ---
#include "display/weather_display.h"
#include "display/time_display.h"

// --- GLOBALNE OBIEKTY ---
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32 Weather Station ===");

  // --- Inicjalizacja TFT ---
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TIME, COLOR_BACKGROUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(FONT_SIZE_LARGE);

  tft.drawString("Laczenie WiFi...", tft.width() / 2, tft.height() / 2);
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  // --- Łączenie z WiFi ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi failed!");
  }

  // --- Konfiguracja czasu ---
  Serial.println("Configuring time...");
  configTzTime(TIMEZONE_INFO, NTP_SERVER);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    tft.fillScreen(TFT_RED);
    tft.drawString("Time Error!", tft.width() / 2, tft.height() / 2);
  } else {
    Serial.println("Time synchronized");
  }
  
  tft.fillScreen(COLOR_BACKGROUND); // Wyczyść ekran
  
  // Pierwsze pobranie pogody
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Getting initial weather...");
    getWeather();
  }
  
  Serial.println("Setup completed");
}

void loop() {
  // --- WYŚWIETLANIE CZASU ---
  displayTime(tft);

  // --- AKTUALIZACJA POGODY (co 10 minut) ---
  unsigned long currentTime = millis();
  if (currentTime - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Updating weather...");
      getWeather();
    }
    lastWeatherUpdate = currentTime;
  }

  // --- WYŚWIETLANIE POGODY ---
  displayWeather(tft);

  delay(1000); // Aktualizuj co sekundę
}