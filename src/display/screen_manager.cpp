#include "managers/ScreenManager.h"
#include "display/screen_manager.h"
#include "display/weather_display.h"
#include "display/forecast_display.h"
#include "display/time_display.h"
#include "display/github_image.h"
#include "config/display_config.h"
#include "sensors/motion_sensor.h"
#include "sensors/dht22_sensor.h"
#include "managers/WeatherCache.h"
#include "managers/TimeDisplayCache.h"
#include "config/location_config.h"
#include "config/hardware_config.h" // Dla FIRMWARE_VERSION

// --- EXTERNAL DEPENDENCIES ---
extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;

// Funkcja z weather_display.cpp
extern void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);

// --- FALLBACK CONSTANTS ---
#ifndef UPDATES_TITLE_Y
  #define UPDATES_CLEAR_Y   130
  #define UPDATES_TITLE_Y   140
  #define UPDATES_DHT22_Y   160
  #define UPDATES_SENSOR_Y  175
  #define UPDATES_WEATHER_Y 190
  #define UPDATES_WEEKLY_Y  205
  #define UPDATES_WIFI_Y    220
#endif

// Singleton instance ScreenManager
static ScreenManager screenManager;

ScreenManager& getScreenManager() {
  return screenManager;
}

void updateScreenManager() {
  if (getDisplayState() == DISPLAY_SLEEPING) {
    return;
  }
  getScreenManager().updateScreenManager();
}

// --- ZMIENNE GLOBALNE I EXTERN ---
extern bool isOfflineMode;
extern void drawNASAImage(TFT_eSPI& tft, bool forceFallback); 
extern void displaySystemStatus(TFT_eSPI& tft);


// === FUNKCJA POMOCNICZA DO PASKÓW POSTĘPU ===
void drawProgressBar(TFT_eSPI& tft, int x, int y, int w, int h, float val, float minVal, float maxVal, uint16_t color) {
    tft.drawRoundRect(x, y, w, h, 4, TFT_DARKGREY);
    
    float range = maxVal - minVal;
    if (range == 0) range = 1; // Zabezpieczenie przez dzieleniem przez 0
    
    float percent = (val - minVal) / range;
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    
    int fillW = (int)((w - 4) * percent);
    
    // Tło paska (ciemny szary/grafit)
    tft.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 2, 0x10A2); 
    
    // Wypełnienie
    if (fillW > 0) {
        tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, color);
    }
}


// ================================================================
// GŁÓWNA FUNKCJA PRZEŁĄCZANIA EKRANÓW (LOGIKA OFFLINE/ONLINE)
// ================================================================
void switchToNextScreen(TFT_eSPI& tft) {
    ScreenManager& mgr = getScreenManager();

    // === 1. TRYB OFFLINE (BEZ WIFI) ===
    if (isOfflineMode) {
        
        ScreenType originalScreen = mgr.getCurrentScreen(); 
        
        // Parzyste = Sensory (trwa 2x dłużej), Nieparzyste = Obrazek
        if ((int)originalScreen % 2 == 0) {
            // --- A. RYSOWANIE SENSORÓW (PROFESJONALNY DASHBOARD) ---
            mgr.setCurrentScreen(SCREEN_LOCAL_SENSORS); 
            mgr.renderCurrentScreen(tft);               
            
            // Zegar na samej górze (nad kreską dashboardu)
            displayTime(tft); 
            
            // W trybie offline nie rysujemy napisów "TRYB OFFLINE" na sensorach,
            // żeby zachować czystość (zegar + dashboard).
            
        } else {
            // --- B. RYSOWANIE OBRAZKA ---
            mgr.setCurrentScreen(SCREEN_IMAGE);         
            drawNASAImage(tft, true); // True = Force Fallback (z pamięci)
            
            // Info o galerii offline na dole obrazka
            tft.fillRect(0, tft.height() - 20, tft.width(), 20, TFT_BLACK);
            tft.setTextSize(1);
            tft.setTextDatum(BC_DATUM);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK); 
            tft.drawString("GALERIA OFFLINE", tft.width() / 2, tft.height() - 5);
        }
        
        mgr.setCurrentScreen(originalScreen); // Przywróć licznik dla timera
        return;
    }

    // === 2. TRYB ONLINE (NORMALNY) ===
    mgr.renderCurrentScreen(tft);
}

void forceScreenRefresh(TFT_eSPI& tft) {
  getScreenManager().forceScreenRefresh(tft);
}

static String local_shortenDescription(String description) {
    String polishDescription = description;
    if (description.indexOf("thunderstorm with heavy rain") >= 0) polishDescription = "Burza z ulewa";
    else if (description.indexOf("thunderstorm with rain") >= 0) polishDescription = "Burza z deszczem";
    else if (description.indexOf("thunderstorm") >= 0) polishDescription = "Burza";
    else if (description.indexOf("drizzle") >= 0) polishDescription = "Mzawka";
    else if (description.indexOf("heavy intensity rain") >= 0) polishDescription = "Ulewa";
    else if (description.indexOf("moderate rain") >= 0) polishDescription = "Umiarkowany deszcz";
    else if (description.indexOf("light rain") >= 0) polishDescription = "Slaby deszcz";
    else if (description.indexOf("rain") >= 0) polishDescription = "Deszcz";
    else if (description.indexOf("snow") >= 0) polishDescription = "Snieg";
    else if (description.indexOf("sleet") >= 0) polishDescription = "Deszcz ze sniegiem";
    else if (description.indexOf("mist") >= 0) polishDescription = "Mgla";
    else if (description.indexOf("fog") >= 0) polishDescription = "Mgla";
    else if (description.indexOf("clear sky") >= 0) polishDescription = "Bezchmurnie";
    else if (description.indexOf("overcast clouds") >= 0) polishDescription = "Pochmurno";
    else if (description.indexOf("broken clouds") >= 0) polishDescription = "Duze zachmurzenie";
    else if (description.indexOf("scattered clouds") >= 0) polishDescription = "Srednie zachmurzenie";
    else if (description.indexOf("few clouds") >= 0) polishDescription = "Male zachmurzenie";
    return polishDescription;
}

static uint16_t local_getWindColor(float windKmh) {
    if (windKmh >= 30.0) return TFT_MAROON;
    else if (windKmh >= 25.0) return TFT_RED;
    else if (windKmh >= 20.0) return TFT_YELLOW;
    else return TFT_WHITE;
}

static uint16_t local_getPressureColor(float pressure) {
    if (pressure < 1000.0) return TFT_ORANGE;
    else if (pressure > 1020.0) return TFT_MAGENTA;
    else return TFT_WHITE;
}

static uint16_t local_getHumidityColor(float humidity) {
    if (humidity < 30.0) return TFT_RED;
    else if (humidity > 90.0) return 0x7800;
    else if (humidity > 85.0) return TFT_PURPLE;
    else return TFT_WHITE;
}

// ================================================================
// IMPLEMENTACJA RENDERING METHODS dla ScreenManager
// ================================================================
void ScreenManager::renderWeatherScreen(TFT_eSPI& tft) {
    tft.fillScreen(COLOR_BACKGROUND);
    displayTime(tft);

    if (!weather.isValid) {
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(1);
        tft.setTextSize(2);
        tft.drawString("BRAK DANYCH", 160, 100);
        return;
    }

    tft.setTextFont(1);

    int height = 175; //(+40)

    // STYL: Ciemny Grafit (0x1082)
    uint16_t CARD_BG = 0x1082;
    // POPRAWKA: Tło tekstu domyślne to CARD_BG (dla prawej kolumny)
    uint16_t TEXT_BG = CARD_BG;  
    uint16_t BORDER_COLOR = TFT_DARKGREY;
    uint16_t LABEL_COLOR = TFT_SILVER;

    uint8_t startY = 5;
    uint8_t startX = 60;
    // =========================================================
    // LEWA KARTA Z TEMPERATURĄ (Tło CZARNE)
    // =========================================================
    tft.fillRoundRect(5, startY, 150, height, 8, TFT_BLACK);  
    tft.drawRoundRect(5, startY, 150, height, 8, BORDER_COLOR); 

    String polishDesc = local_shortenDescription(weather.description);
    polishDesc.toUpperCase();

    tft.setTextDatum(MC_DATUM);
    drawWeatherIcon(tft, startX - 5, WEATHER_CARD_TEMP_Y_OFFSET - 10, weather.description, weather.icon);

    uint16_t tempColor = TFT_WHITE;
    if (weather.temperature < 0 || weather.feelsLike < 0) tempColor = TFT_CYAN;
    else if (weather.temperature > 30) tempColor = TFT_RED;
    else if (weather.temperature > 25) tempColor = TFT_ORANGE;

    // Tutaj używamy jawnie TFT_BLACK, bo karta jest czarna
    tft.setTextColor(tempColor, TFT_BLACK); 
    tft.setTextSize(5);
    tft.setTextDatum(MC_DATUM);
    String tempStr = String((int)round(weather.temperature));
    tft.drawString(tempStr, 80, startY + 60 + WEATHER_CARD_TEMP_Y_OFFSET);

    // Symbol "C" usunięty na żądanie użytkownika
    // tft.setTextSize(2);
    // int tempWidth = tft.textWidth(tempStr) * 5;
    // tft.drawString("C", 80 + (tempWidth/2) + 10, startY + 50 + WEATHER_CARD_TEMP_Y_OFFSET);

    uint16_t descColor = TFT_CYAN;
    if (polishDesc.indexOf("BURZA") >= 0) descColor = TFT_RED;
    else if (polishDesc == "BEZCHMURNIE") descColor = TFT_YELLOW;
    else if (polishDesc == "MGLA") descColor = TFT_WHITE;

    // Jawnie TFT_BLACK
    tft.setTextColor(descColor, TFT_BLACK);
    tft.setTextSize(2);
    if (tft.textWidth(polishDesc) > 140) tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(polishDesc, 80, startY + 95 + WEATHER_CARD_TEMP_Y_OFFSET);

    // Jawnie TFT_BLACK
    tft.setTextColor(LABEL_COLOR, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("ODCZUWALNA:", 80, startY + 116 + WEATHER_CARD_TEMP_Y_OFFSET);
    
    // Jawnie TFT_BLACK (bez jednostki "C" - usunięta na żądanie użytkownika)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString(String((int)round(weather.feelsLike)), 80, startY + 135 + WEATHER_CARD_TEMP_Y_OFFSET);

    // =========================================================
    // PRAWA KOLUMNA (Tło GRAFITOWE - CARD_BG)
    // =========================================================
    uint8_t rowH = 39; uint8_t gap = 6; uint8_t rightX = 165; uint8_t rightW = 150;

    // --- 1. WILGOTNOŚĆ ---
    uint8_t y1 = startY;
    tft.fillRoundRect(rightX, y1, rightW, rowH, 6, CARD_BG);
    tft.drawRoundRect(rightX, y1, rightW, rowH, 6, BORDER_COLOR);
    
    // Tutaj używamy TEXT_BG (który teraz jest równy CARD_BG), więc kwadraty znikną!
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("WILGOTNOSC", rightX + 5, y1 + 5);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(local_getHumidityColor(weather.humidity), TEXT_BG);
    tft.setTextSize(3);
    tft.drawString(String((int)weather.humidity) + "%", rightX + rightW - 5, y1 + 12);

    // --- 2. WIATR ---
    uint8_t y2 = y1 + rowH + gap;
    tft.fillRoundRect(rightX, y2, rightW, rowH, 6, CARD_BG);
    tft.drawRoundRect(rightX, y2, rightW, rowH, 6, BORDER_COLOR);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("WIATR", rightX + 5, y2 + 5);

    float windKmh = weather.windSpeed * 3.6;
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(local_getWindColor(windKmh), TEXT_BG);
    tft.setTextSize(3);
    tft.drawString(String((int)round(windKmh)), rightX + rightW - 35, y2 + 12);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    tft.drawString("km/h", rightX + rightW - 5, y2 + 27);

    // --- 3. CIŚNIENIE ---
    uint8_t y3 = y2 + rowH + gap;
    tft.fillRoundRect(rightX, y3, rightW, rowH, 6, CARD_BG);
    tft.drawRoundRect(rightX, y3, rightW, rowH, 6, BORDER_COLOR);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("CISNIENIE", rightX + 5, y3 + 5);
    
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(local_getPressureColor(weather.pressure), TEXT_BG);
    tft.setTextSize(3);
    if (weather.pressure > 999) tft.setTextSize(2);
    tft.drawString(String((int)weather.pressure), rightX + rightW - 30, y3 + 12);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TEXT_BG);
    tft.drawString("hPa", rightX + rightW - 5, y3 + 27);

    tft.setTextSize(1);

        // --- 4. OPADY ---
    uint8_t y4 = y3 + rowH + gap;
    tft.fillRoundRect(rightX, y4, rightW, rowH , 6, CARD_BG);
    tft.drawRoundRect(rightX, y4, rightW, rowH , 6, BORDER_COLOR);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(LABEL_COLOR, TEXT_BG);
    tft.setTextSize(1);
    tft.drawString("SZANSA NA OPADY", rightX + 5, y4 + 5);

    String rainVal = "--";
    uint16_t rainColor = LABEL_COLOR;
    if (forecast.isValid && forecast.count > 0) {
        uint8_t chance = forecast.items[0].precipitationChance;
        rainVal = String(chance) + "%";
        rainColor = (chance > 0) ? TFT_SKYBLUE : LABEL_COLOR;
    }
    // tft.setTextColor(rainColor, TEXT_BG);
    // tft.setTextSize(2);
    // tft.drawString(rainVal, rightX + rightW - 35, y4+ 12);

    tft.setTextDatum(TR_DATUM);

    tft.setTextSize(2);
    tft.drawString(rainVal, rightX + rightW - 5, y4 + 12);


    updateWeatherCache();
}



void ScreenManager::renderForecastScreen(TFT_eSPI& tft) {
  displayForecast(tft);
}

// === EKRAN 3: WEEKLY FORECAST (Pełna implementacja) ===
// === EKRAN 3: WEEKLY FORECAST (Z poprawioną stopką) ===
void ScreenManager::renderWeeklyScreen(TFT_eSPI& tft) {
  Serial.println("📱 Ekran wyczyszczony - rysowanie: WEEKLY");
  
  // Sprawdzenie danych
  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak danych", 160, 80);
    tft.setTextSize(1);
    tft.drawString("prognoza tygodniowa", 160, 110);
    return;
  }
  
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  uint8_t availableHeight = 200; 
  uint8_t startY = 15;
  uint8_t rowHeight = weeklyForecast.count > 0 ? (availableHeight / weeklyForecast.count) : 35;
  
  uint8_t textOffset = (rowHeight - 16) / 2; 
  if (textOffset < 0) textOffset = 0;
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    DailyForecast& day = weeklyForecast.days[i];
    int rawY = startY + (i * rowHeight);
    int y = rawY + textOffset; 
    
    // 1. Dzień
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(day.dayName, 10, y);
    
    // 2. Ikona
    uint8_t iconX = 55;
    int iconY = rawY - (rowHeight / 4) + textOffset;
    
    String condition = "unknown";
    if (day.icon.indexOf("01") >= 0) condition = "clear sky";
    else if (day.icon.indexOf("02") >= 0) condition = "few clouds";
    else if (day.icon.indexOf("03") >= 0) condition = "scattered clouds";
    else if (day.icon.indexOf("04") >= 0) condition = "overcast clouds";
    else if (day.icon.indexOf("09") >= 0) condition = "shower rain";
    else if (day.icon.indexOf("10") >= 0) condition = "rain";
    else if (day.icon.indexOf("11") >= 0) condition = "thunderstorm";
    else if (day.icon.indexOf("13") >= 0) condition = "snow";
    else if (day.icon.indexOf("50") >= 0) condition = "mist";
    
    drawWeatherIcon(tft, iconX, iconY, condition, day.icon);
    
    // 3. Temperatury Min/Max - dynamiczne pozycjonowanie bez spacji
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    
    uint8_t tempX = 120;  // Start position
    
    // Min temp
    String minStr = String((int)round(day.tempMin));
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(minStr, tempX, y);
    tempX += tft.textWidth(minStr);  // Przesuń za tekst
    
    // Separator
    tft.setTextColor(TFT_WHITE);
    tft.drawString("/", tempX, y);
    tempX += tft.textWidth("/");  // Przesuń za separator
    
    // Max temp
    String maxStr = String((int)round(day.tempMax));
    tft.setTextColor(TFT_WHITE);
    tft.drawString(maxStr, tempX, y);
    tempX += tft.textWidth(maxStr);  // Oblicz koniec temperatur
    
    // 4. Skalowanie czcionki dla Wiatru/Opadów (uproszczone - bez warunku minusów)
    int8_t tMinInt = (int8_t)round(day.tempMin);
    int8_t tMaxInt = (int8_t)round(day.tempMax);
    
    uint8_t wideValuesCount = (abs(tMinInt) >= 10) + 
                          (abs(tMaxInt) >= 10) + 
                          (day.windMin >= 10) + 
                          (day.windMax >= 10) + 
                          (day.precipitationChance >= 10);

    // Jeśli 4 lub więcej wartości jest szerokich -> użyj małej czcionki
    bool useSmallFont = (wideValuesCount >= 4);

    uint8_t dataTextSize = useSmallFont ? 1 : 2;
    uint8_t yOffsetData = useSmallFont ? 5 : 0;
    uint8_t unitCorrection = useSmallFont ? 0 : 5; 

    // 5. Wiatr - dynamiczna pozycja PO temperaturach (z marginesem)
    tft.setTextSize(dataTextSize);
    int currentX = tempX + 10;  // Start 10px po końcu temperatur (dynamicznie!)

    int windY = y;
    if (useSmallFont)
    windY = y + yOffsetData + unitCorrection;

    tft.setTextColor(TFT_DARKGREY); 
    String minWind = String((int)round(day.windMin));
    tft.drawString(minWind, currentX, windY);
    currentX += tft.textWidth(minWind); 

    tft.setTextColor(TFT_WHITE);
    String sep = "-"; 
    tft.drawString(sep, currentX, windY);
    currentX += tft.textWidth(sep);

    String maxWind = String((int)round(day.windMax));
    tft.drawString(maxWind, currentX, windY);
    currentX += tft.textWidth(maxWind);

    tft.setTextSize(1);
    tft.setTextColor(TFT_SILVER);
    
    tft.drawString("km/h", currentX + 2, y + yOffsetData + unitCorrection);
    
    // 6. Opady - ZAWSZE normalna czcionka (size 2)
    tft.setTextColor(0x001F);
    tft.setTextDatum(TR_DATUM);
    if ((day.precipitationChance) >= 100)
    tft.setTextSize(1); 
    else   
    tft.setTextSize(2); 

    tft.drawString(String(day.precipitationChance) + "%", 315, y );
    tft.setTextDatum(TL_DATUM); 
  }
  
  // === FOOTER Z LOKALIZACJĄ (POPRAWIONY) ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM); // Punkt odniesienia: Środek Góry tekstu
  
  // Czyścimy dół ekranu (od Y=210 do końca)
  tft.fillRect(0, 210, 320, 40, COLOR_BACKGROUND);
  
  if (locationManager.isLocationSet()) {
    WeatherLocation loc = locationManager.getCurrentLocation();
    String locationText;
    
    // Budowanie nazwy
    if (loc.displayName.length() > 0 && loc.displayName != loc.cityName) {
        locationText = loc.cityName + ", " + loc.displayName;
    } else {
        locationText = loc.cityName;
    }

    // --- ZABEZPIECZENIE DŁUGOŚCI (PIXEL PERFECT) ---
    // Sprawdzamy szerokość tekstu w pikselach, a nie w znakach.
    // Max szerokość = 310px (zostawiamy 5px marginesu po bokach)
    int maxWidth = 310;
    if (tft.textWidth(locationText) > maxWidth) 
      tft.setTextSize(1);

    
    // Rysowanie obniżone o 10px (było 205, jest 215)
    tft.drawString(locationText, 160, 215); 
    
  } else {
    tft.drawString("Brak lokalizacji", 160, 215);
  }
}

// === EKRAN 4: LOCAL SENSORS (PROFESJONALNY DASHBOARD) ===
// === EKRAN 4: LOCAL SENSORS (DASHBOARD RESPONSYWNY) ===
void ScreenManager::renderLocalSensorsScreen(TFT_eSPI& tft) {
  Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS (PRO)");
  
  tft.fillScreen(COLOR_BACKGROUND);

  // Pobierz dane
  DHT22Data dhtData = getDHT22Data();
  float temp = dhtData.temperature;
  float hum = dhtData.humidity;
  bool isValid = dhtData.isValid;

  // === KONFIGURACJA UKŁADU ===
  bool isCompactMode = !isOfflineMode; // Online = Kompaktowy (żeby zmieścić stopkę)
  
  // Ustawienia geometryczne
  uint8_t cardY = 55;       // Zaczynamy zaraz pod nagłówkiem (linia jest na 45)
  int cardH;
  
  if (isCompactMode) {
      cardH = 70;       // Kończymy na Y=125 (Stopka zaczyna się od 130)
  } else {
      cardH = 140;      // Offline: Duże karty
  }
  
  uint8_t cardW = 135;      
  uint8_t card1_X = 20;     
  uint8_t card2_X = 165;    

  // --- NAGŁÓWEK (ZAWSZE PROFESJONALNY) ---
  tft.drawFastHLine(20, 45, 280, TFT_DARKGREY); 
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("WARUNKI W POMIESZCZENIU", tft.width() / 2, 35);


  // =========================================================
  // KARTA 1: TEMPERATURA
  // =========================================================
  tft.fillRoundRect(card1_X, cardY, cardW, cardH, 8, 0x1082); 
  tft.drawRoundRect(card1_X, cardY, cardW, cardH, 8, TFT_DARKGREY); 

  // Etykieta
  tft.setTextColor(TFT_ORANGE, 0x1082);
  tft.setTextDatum(MC_DATUM); 
  tft.setTextSize(1);
  uint8_t labelY = cardY + 15; // Stała pozycja etykiety
  tft.drawString("TEMP", card1_X + cardW/2, labelY);
  
  if (isValid) {
      uint16_t tempColor = TFT_GREEN;
      if (temp < 18) tempColor = TFT_CYAN;     
      if (temp > 24) tempColor = TFT_ORANGE;    
      if (temp > 28) tempColor = TFT_RED;       
      
      tft.setTextColor(tempColor, 0x1082);
      
      // Pozycjonowanie wartości (zależne od wysokości karty)
      int valY = isCompactMode ? cardY + 40 : cardY + 60;
      
      tft.setTextFont(4); 
      tft.drawString(String(temp, 1), card1_X + cardW/2, valY);
      
      // Jednostka (PRZYWRÓCONA - jest miejsce w LOCAL SENSORS)
      tft.setTextFont(2); 
      if (isCompactMode) {
          // W trybie kompaktowym jednostka obok liczby
          tft.drawString("'C", card1_X + cardW/2 + 45, valY - 5);
      } else {
          // W trybie dużym pod spodem
          tft.drawString("st C", card1_X + cardW/2, cardY + 90);
      }
      
      // Pasek postępu
      if (!isCompactMode) {
          drawProgressBar(tft, card1_X + 15, cardY + 110, cardW - 30, 10, temp, 0, 40, tempColor);
      } else {
          // Wersja mini na dole karty
          drawProgressBar(tft, card1_X + 10, cardY + cardH - 8, cardW - 20, 4, temp, 0, 40, tempColor);
      }
      
  } else {
      tft.setTextColor(TFT_RED, 0x1082);
      tft.setTextFont(4); 
      tft.drawString("--.-", card1_X + cardW/2, cardY + cardH/2);
  }


  // =========================================================
  // KARTA 2: WILGOTNOŚĆ
  // =========================================================
  tft.fillRoundRect(card2_X, cardY, cardW, cardH, 8, 0x1082);
  tft.drawRoundRect(card2_X, cardY, cardW, cardH, 8, TFT_DARKGREY);

  tft.setTextColor(TFT_CYAN, 0x1082);
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextDatum(MC_DATUM);
  tft.drawString("WILGOTNOSC", card2_X + cardW/2, labelY);
  
  if (isValid) {
      uint16_t humColor = TFT_GREEN;
      if (hum < 30) humColor = TFT_YELLOW;  
      if (hum > 60) humColor = TFT_BLUE;    
      
      tft.setTextColor(humColor, 0x1082);
      
      int valY = isCompactMode ? cardY + 40 : cardY + 60;

      tft.setTextFont(4);
      tft.drawString(String((int)hum), card2_X + cardW/2, valY);
      
      tft.setTextFont(2);
      if (isCompactMode) {
          tft.drawString("%", card2_X + cardW/2 + 35, valY - 5);
      } else {
          tft.drawString("% RH", card2_X + cardW/2, cardY + 90);
      }
      
      if (!isCompactMode) {
          drawProgressBar(tft, card2_X + 15, cardY + 110, cardW - 30, 10, hum, 0, 100, humColor);
      } else {
          drawProgressBar(tft, card2_X + 10, cardY + cardH - 8, cardW - 20, 4, hum, 0, 100, humColor);
      }
      
  } else {
      tft.setTextColor(TFT_RED, 0x1082);
      tft.setTextFont(4); 
      tft.drawString("--", card2_X + cardW/2, cardY + cardH/2);
  }
  
  
  // =========================================================
  // FOOTER (STOPKA SYSTEMOWA) - TYLKO W TRYBIE ONLINE
  // =========================================================
  if (!isOfflineMode) {
      
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextFont(1);
      
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      
      // Czyścimy obszar stopki
      tft.fillRect(0, UPDATES_CLEAR_Y, 320, 240 - UPDATES_CLEAR_Y, COLOR_BACKGROUND);
      
      // Etykieta
      tft.setTextDatum(TC_DATUM);
      tft.setTextColor(TFT_CYAN);
      tft.drawString("STATUS SYSTEMU:", 160, UPDATES_TITLE_Y);
      
      // 1. Status DHT
      String dhtStatus = "DHT22: " + dhtData.status;
      uint16_t statusColor = dhtData.isValid ? TFT_GREEN : TFT_RED;
      tft.setTextColor(statusColor);
      tft.drawString(dhtStatus, 160, UPDATES_DHT22_Y);
      
      // 2. Info o sensorze
      tft.setTextColor(TFT_DARKGREY);
      tft.drawString("Odczyt sensora: co 2s", 160, UPDATES_SENSOR_Y);
      
      // 3. Pogoda bieżąca (OSOBNA LINIA)
      unsigned long weatherAge = (millis() - lastWeatherCheckGlobal) / 1000;
      String wPrefix = "Pogoda: ";
      String wTime;
      if (weatherAge < 60) wTime = String(weatherAge) + "s temu";
      else if (weatherAge < 3600) wTime = String(weatherAge / 60) + "min temu";
      else wTime = String(weatherAge / 3600) + "h temu";
      String wSuffix = " (co 10min)";
      
      int wTotalWidth = tft.textWidth(wPrefix + wTime + wSuffix);
      int wX = 160 - (wTotalWidth / 2);
      
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(TFT_DARKGREY); tft.drawString(wPrefix, wX, UPDATES_WEATHER_Y);
      wX += tft.textWidth(wPrefix);
      tft.setTextColor(TFT_WHITE);    tft.drawString(wTime, wX, UPDATES_WEATHER_Y);
      wX += tft.textWidth(wTime);
      tft.setTextColor(TFT_DARKGREY); tft.drawString(wSuffix, wX, UPDATES_WEATHER_Y);
      
      // 4. Prognoza weekly (OSOBNA LINIA)
      unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
      String fPrefix = "Pogoda tyg.: ";
      String fTime;
      if (weeklyAge < 60) fTime = String(weeklyAge) + "s temu";
      else if (weeklyAge < 3600) fTime = String(weeklyAge / 60) + "min temu";
      else fTime = String(weeklyAge / 3600) + "h temu";
      String fSuffix = " (co 4h)";
      
      int fTotalWidth = tft.textWidth(fPrefix + fTime + fSuffix);
      int fX = 160 - (fTotalWidth / 2);
      
      tft.setTextColor(TFT_DARKGREY); tft.drawString(fPrefix, fX, UPDATES_WEEKLY_Y);
      fX += tft.textWidth(fPrefix);
      tft.setTextColor(TFT_WHITE);    tft.drawString(fTime, fX, UPDATES_WEEKLY_Y);
      fX += tft.textWidth(fTime);
      tft.setTextColor(TFT_DARKGREY); tft.drawString(fSuffix, fX, UPDATES_WEEKLY_Y);
      
      // 5. Stan WiFi
      String wifiStatus;
      if (WiFi.status() == WL_CONNECTED) {
        int8_t rssi = WiFi.RSSI();
        uint8_t quality = 0;
        if(rssi <= -100) quality = 0;
        else if(rssi >= -50) quality = 100;
        else quality = 2 * (rssi + 100);
        wifiStatus = "WiFi: " + String(WiFi.SSID()) + " (" + String(quality) + "%)";
      } else {
        wifiStatus = "WiFi: Rozlaczony";
      }
      
      tft.setTextDatum(TC_DATUM);
      tft.setTextColor(TFT_DARKGREY);
      tft.drawString(wifiStatus, 160, UPDATES_WIFI_Y);

      // Stopka wersji (Prawy Dolny Róg)
      tft.setTextDatum(BR_DATUM); 
      tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND); 
      String versionText = "v" + String(FIRMWARE_VERSION);
      tft.drawString(versionText, tft.width() - 5, tft.height() - 5);
  }
  
  // Reset ustawień
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  displayGitHubImage(tft);
}

void ScreenManager::resetWeatherAndTimeCache() {
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}