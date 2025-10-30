#include "display/weather_display.h"
#include "display/weather_icons.h"
#include "config/display_config.h"

// Definicje zmiennych cache
float weatherCachePrev_temperature = -999.0;
float weatherCachePrev_feelsLike = -999.0;
float weatherCachePrev_humidity = -999.0;
float weatherCachePrev_windSpeed = -999.0;
float weatherCachePrev_pressure = -999.0;
String weatherCachePrev_description = "";
String weatherCachePrev_icon = "";

// Funkcja sprawdzająca czy dane pogodowe się zmieniły
bool hasWeatherChanged() {
  return (weather.temperature != weatherCachePrev_temperature ||
          weather.feelsLike != weatherCachePrev_feelsLike ||
          weather.humidity != weatherCachePrev_humidity ||
          weather.windSpeed != weatherCachePrev_windSpeed ||
          weather.pressure != weatherCachePrev_pressure ||
          weather.description != weatherCachePrev_description ||
          weather.icon != weatherCachePrev_icon);
}

// Funkcja zapisująca aktualne dane do cache
void updateWeatherCache() {
  weatherCachePrev_temperature = weather.temperature;
  weatherCachePrev_feelsLike = weather.feelsLike;
  weatherCachePrev_humidity = weather.humidity;
  weatherCachePrev_windSpeed = weather.windSpeed;
  weatherCachePrev_pressure = weather.pressure;
  weatherCachePrev_description = weather.description;
  weatherCachePrev_icon = weather.icon;
}

// Funkcja wybierająca kolor wiatru na podstawie prędkości
uint16_t getWindColor(float windKmh) {
  if (windKmh >= 25.0) {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - EXTREME (bordowy)");
    return COLOR_WIND_EXTREME;    // 25+ km/h - bordowy (bardzo silny)
  } else if (windKmh >= 20.0) {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - STRONG (czerwony)");
    return COLOR_WIND_STRONG;     // 20-25 km/h - czerwony (silny)
  } else if (windKmh >= 15.0) {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - MODERATE (żółty)");
    return COLOR_WIND_MODERATE;   // 15-20 km/h - żółty (umiarkowany)
  } else {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - CALM (biały)");
    return COLOR_WIND_CALM;       // 0-15 km/h - biały (spokojny)
  }
}

// Funkcja wybierająca kolor ciśnienia na podstawie wartości
uint16_t getPressureColor(float pressure) {
  if (pressure < 1000.0) {
    Serial.println("Pressure: " + String(pressure, 0) + "hPa - LOW (pomarańczowy)");
    return COLOR_PRESSURE_LOW;    // <1000 hPa - pomarańczowy (niskie, deszcz)
  } else if (pressure > 1020.0) {
    Serial.println("Pressure: " + String(pressure, 0) + "hPa - HIGH (magenta)");
    return COLOR_PRESSURE_HIGH;   // >1020 hPa - magenta (wysokie, pogodnie)
  } else {
    Serial.println("Pressure: " + String(pressure, 0) + "hPa - NORMAL (biały)");
    return COLOR_PRESSURE_NORMAL; // 1000-1020 hPa - biały (normalne)
  }
}

String shortenDescription(String description) {
  // DEBUG - wypisz oryginalny opis w Serial
  Serial.println("Opis pogody ORYGINALNY: '" + description + "'");
  
  String shortDescription = description;
  
  // Zamiana polskich znaków na ASCII (dla TFT)
  shortDescription.replace("ą", "a");
  shortDescription.replace("ć", "c");
  shortDescription.replace("ę", "e");
  shortDescription.replace("ł", "l");
  shortDescription.replace("ń", "n");
  shortDescription.replace("ó", "o");
  shortDescription.replace("ś", "s");
  shortDescription.replace("ź", "z");
  shortDescription.replace("ż", "z");
  
  // Skracanie opisów z API (&lang=pl) - BEZ polskich znaków
  if (shortDescription.indexOf("zachmurzenie duze") >= 0) {
    shortDescription = "Duze chmury";
  } else if (shortDescription.indexOf("zachmurzenie male") >= 0) {
    shortDescription = "Male chmury";
  } else if (shortDescription.indexOf("zachmurzenie umiarkowane") >= 0) {
    shortDescription = "Umiark. chmury";
  } else if (shortDescription.indexOf("zachmurzenie") >= 0) {
    shortDescription = "Zachmurzenie";
  } else if (shortDescription.indexOf("pochmurnie") >= 0) {
    shortDescription = "Pochmurnie";
  } else if (shortDescription.indexOf("bezchmurnie") >= 0) {
    shortDescription = "Bezchmurnie";
  } else if (shortDescription.indexOf("slonecznie") >= 0) {
    shortDescription = "Slonecznie";
  } else if (shortDescription.indexOf("deszcz lekki") >= 0) {
    shortDescription = "Lekki deszcz";
  } else if (shortDescription.indexOf("deszcz silny") >= 0) {
    shortDescription = "Silny deszcz";
  } else if (shortDescription.indexOf("deszcz") >= 0) {
    shortDescription = "Deszcz";
  } else if (shortDescription.indexOf("snieg") >= 0) {
    shortDescription = "Snieg";
  } else if (shortDescription.indexOf("mgla") >= 0) {
    shortDescription = "Mgla";
  } else if (shortDescription.indexOf("burza") >= 0) {
    shortDescription = "Burza";
  } else {
    // Jeśli nic nie pasuje, skróć do 12 znaków (bez polskich znaków)
    if (shortDescription.length() > 12) {
      shortDescription = shortDescription.substring(0, 12);
    }
  }
  
  Serial.println("Wyswietlany opis: '" + shortDescription + "'");
  return shortDescription;
}

void displayWeather(TFT_eSPI& tft) {
  if (!weather.isValid) return;
  
  // Sprawdź czy dane pogodowe się zmieniły
  if (!hasWeatherChanged()) {
    return; // Nie ma zmian - nie rysuj ponownie!
  }
  
  Serial.println("Weather data changed - redrawing display");
  
  // Pozycja pogody - GÓRA na całej szerokości
  int x = WEATHER_AREA_X;
  int y = WEATHER_AREA_Y;
  
  // Wyczyść CAŁY obszar pogody gdy są zmiany
  tft.fillRect(x, y, WEATHER_AREA_WIDTH, WEATHER_AREA_HEIGHT, COLOR_BACKGROUND);
  
  // Ustawienia tekstu
  tft.setTextSize(FONT_SIZE_LARGE);
  tft.setTextDatum(TL_DATUM);
  
  // Ikona pogody
  drawWeatherIcon(tft, x + ICON_X_OFFSET, y + ICON_Y_OFFSET, weather.description, weather.icon);
  
  // Temperatura z odczuwalną w nawiasie - wyczyść całą linię
  tft.fillRect(x + TEMP_X_OFFSET, y + TEMP_Y_OFFSET, 200, 25, COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEMPERATURE, COLOR_BACKGROUND);
  String tempStr = String(weather.temperature, 1) + "'C(" + String(weather.feelsLike, 1) + "'C)";
  tft.drawString(tempStr, x + TEMP_X_OFFSET, y + TEMP_Y_OFFSET);
  
  // Opis pogody - wyczyść całą linię
  tft.fillRect(x + DESC_X_OFFSET, y + DESC_Y_OFFSET, 250, 25, COLOR_BACKGROUND);
  tft.setTextColor(COLOR_DESCRIPTION, COLOR_BACKGROUND);
  String shortDescription = shortenDescription(weather.description);
  tft.drawString(shortDescription, x + DESC_X_OFFSET, y + DESC_Y_OFFSET);
  
  // Wilgotność - wyczyść lewą część linii
  tft.fillRect(x + HUMIDITY_X_OFFSET, y + HUMIDITY_Y_OFFSET, 120, 25, COLOR_BACKGROUND);
  tft.setTextColor(COLOR_HUMIDITY, COLOR_BACKGROUND);
  String humStr = "Wilg: " + String(weather.humidity, 0) + "%";
  tft.drawString(humStr, x + HUMIDITY_X_OFFSET, y + HUMIDITY_Y_OFFSET);
  
  // Wiatr - wyczyść linię wiatru
  float windKmh = weather.windSpeed * 3.6;
  uint16_t windColor = getWindColor(windKmh);
  tft.fillRect(x + WIND_X_OFFSET, y + WIND_Y_OFFSET, 200, 25, COLOR_BACKGROUND);
  tft.setTextColor(windColor, COLOR_BACKGROUND);
  String windStr = "Wiatr: " + String(windKmh, 1) + " km/h";
  tft.drawString(windStr, x + WIND_X_OFFSET, y + WIND_Y_OFFSET);
  
  // Ciśnienie - wyczyść linię ciśnienia (POD wiatrem) - szerszy obszar dla długich liczb
  uint16_t pressureColor = getPressureColor(weather.pressure);
  tft.fillRect(x + PRESSURE_X_OFFSET, y + PRESSURE_Y_OFFSET, 250, 25, COLOR_BACKGROUND);
  tft.setTextColor(pressureColor, COLOR_BACKGROUND);
  String pressureStr = "Cisn: " + String(weather.pressure, 0) + " hPa";
  tft.drawString(pressureStr, x + PRESSURE_X_OFFSET, y + PRESSURE_Y_OFFSET);
  
  // Zaktualizuj cache po narysowaniu
  updateWeatherCache();
}