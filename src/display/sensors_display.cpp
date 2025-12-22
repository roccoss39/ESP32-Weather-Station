#include "sensors_display.h"
#include "config/display_config.h"
#include "sensors/dht22_sensor.h"
#include <TFT_eSPI.h>

// Extern dependencies
extern DHT22Sensor dht22;

// Extern global function
extern DHT22Data getDHT22Data();

// ================================================================
// FUNKCJA WYŚWIETLANIA SENSORÓW LOKALNYCH (DHT22)
// ================================================================

void displaySensors(TFT_eSPI& tft) {
  Serial.println("📱 Ekran wyczyszczony - rysowanie: LOCAL SENSORS");

  // Pobierz dane z DHT22
  DHT22Data dhtData = getDHT22Data();
  float temp = dhtData.temperature;
  float humidity = dhtData.humidity;
  bool isValid = dhtData.isValid;

  // === GÓRNY NAGŁÓWEK Z CZASEM === (rysowany w screen_manager - displayTime())
  
  // === SEKCJA GŁÓWNA: DANE Z DHT22 ===
  uint8_t cardY = 55;       // Zaczynamy zaraz pod nagłówkiem (linia jest na 45)
  uint8_t cardH = 120;      // Wysokość karty ZWIĘKSZONA (było 110)
  
  uint8_t cardW = 135;      
  uint8_t card1_X = 20;     
  uint8_t card2_X = 165;    

  // --- TRYB KOMPAKTOWY LUB PEŁNY ---
  bool isCompactMode = false;  // FALSE = tryb DUŻY (2 karty obok)
                               // TRUE = tryb KOMPAKTOWY (1 karta)

  // === KARTA 1: TEMPERATURA ===
  tft.fillRoundRect(card1_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
  tft.drawRoundRect(card1_X, cardY, cardW, cardH, 8, TFT_WHITE);

  uint8_t labelY = cardY + 15; // Stała pozycja etykiety
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM); // Top Center
  tft.drawString("TEMP", card1_X + cardW/2, labelY);

  // Wartość - DUŻA W ŚRODKU
  String tempStr = (isValid && temp > -100) ? String(temp, 1) : "--.-";
  
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(5); // DUŻA czcionka
  tft.setTextDatum(MC_DATUM); // Middle Center
  
  // Pozycjonowanie w środku karty
  int valY = cardY + cardH/2 + 5;
  if (isCompactMode) valY = cardY + cardH/2;
  
  tft.drawString(tempStr, card1_X + cardW/2, valY);

  // Jednostka (mniejsza, z boku lub pod spodem)
  tft.setTextFont(2); 
  if (isCompactMode) {
      // W trybie kompaktowym jednostka obok liczby
      tft.drawString("'C", card1_X + cardW/2 + 45, valY - 5);
  } else {
      // W trybie dużym pod spodem
      tft.drawString("st C", card1_X + cardW/2, cardY + 90);
  }

  // === KARTA 2: WILGOTNOŚĆ ===
  if (!isCompactMode) {  // Tylko w trybie dużym
      tft.fillRoundRect(card2_X, cardY, cardW, cardH, 8, TFT_DARKGREY);
      tft.drawRoundRect(card2_X, cardY, cardW, cardH, 8, TFT_WHITE);

      tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
      tft.setTextSize(2);
      tft.setTextDatum(TC_DATUM);
      tft.drawString("WILG", card2_X + cardW/2, labelY);

      String humStr = (isValid && humidity >= 0) ? String((int)humidity) : "--";
      
      tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
      tft.setTextSize(5);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(humStr, card2_X + cardW/2, valY);

      tft.setTextFont(2);
      tft.drawString("%", card2_X + cardW/2, cardY + 90);
  }

  // === SEKCJA STATUSU (DÓŁ EKRANU) ===
  uint8_t statusY = cardY + cardH + 20; // 20px pod kartami
  tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);

  // Wyświetl status z DHT22Data
  String statusMsg = dhtData.status;
  uint16_t statusColor = TFT_DARKGREY;
  
  if (dhtData.isValid) {
      statusColor = TFT_GREEN;
  } else if (statusMsg.indexOf("komunikacji") >= 0) {
      statusColor = TFT_RED;
  } else if (statusMsg.indexOf("zakresem") >= 0) {
      statusColor = TFT_ORANGE;
  }
  
  tft.setTextColor(statusColor, COLOR_BACKGROUND);
  tft.drawString("Status: " + statusMsg, 160, statusY);
  
  // Timestamp ostatniej aktualizacji
  tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
  tft.setTextSize(1);
  
  if (dhtData.lastUpdate > 0) {
    unsigned long secondsAgo = (millis() - dhtData.lastUpdate) / 1000;
    String timeStr = "Ostatnia aktualizacja: " + String(secondsAgo) + "s temu";
    tft.drawString(timeStr, 160, statusY + 15);
  }
}
