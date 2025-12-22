#include "display/weather_display.h"
#include "managers/WeatherCache.h"

// Singleton instance WeatherCache  
static WeatherCache weatherCache;

WeatherCache& getWeatherCache() {
  return weatherCache;
}
#include "display/weather_icons.h"
#include "config/display_config.h"
#include "weather/forecast_data.h"

// Definicje zmiennych cache
// ❌ USUNIĘTE: 7 extern variables zastąpione WeatherCache class

// --- OOP CACHE FUNCTIONS - Używają WeatherCache class ---

bool hasWeatherChanged() {
  return getWeatherCache().hasChanged(weather);
}

void updateWeatherCache() {
  getWeatherCache().updateCache(weather);
}

// Funkcja wybierająca kolor wiatru na podstawie prędkości
uint16_t getWindColor(float windKmh) {
  if (windKmh >= 30.0) {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - EXTREME (bordowy)");
    return COLOR_WIND_EXTREME;    // 25+ km/h - bordowy (bardzo silny)
  } else if (windKmh >= 25.0) {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - STRONG (czerwony)");
    return COLOR_WIND_STRONG;     // 20-25 km/h - czerwony (silny)
  } else if (windKmh >= 20.0) {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - MODERATE (zolty)");
    return COLOR_WIND_MODERATE;   // 15-20 km/h - zolty (umiarkowany)
  } else {
    Serial.println("Wind: " + String(windKmh, 1) + "km/h - CALM (bialy)");
    return COLOR_WIND_CALM;       // 0-15 km/h - bialy (spokojny)
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
    Serial.println("Pressure: " + String(pressure, 0) + "hPa - NORMAL (bialy)");
    return COLOR_PRESSURE_NORMAL; // 1000-1020 hPa - bialy (normalne)
  }
}

// Funkcja wybierająca kolor wilgotności na podstawie wartości
uint16_t getHumidityColor(float humidity) {
  if (humidity < 30.0) {
    Serial.println("Humidity: " + String(humidity, 0) + "% - DRY (ciemny czerwony)");
    return COLOR_HUMIDITY_DRY;     // <30% - ciemny czerwony (za sucho)
  } else if (humidity > 90.0) {
    Serial.println("Humidity: " + String(humidity, 0) + "% - WET (brązowy)");
    return COLOR_HUMIDITY_WET;     // >85% - brązowy (bardzo wilgotno)
  } else if (humidity > 85.0) {
    Serial.println("Humidity: " + String(humidity, 0) + "% - HUMID (fioletowy)");
    return COLOR_HUMIDITY_HUMID;   // 80-85% - zolty (duszno)
  } else {
    Serial.println("Humidity: " + String(humidity, 0) + "% - COMFORT (bialy)");
    return COLOR_HUMIDITY_COMFORT; // 30-70% - bialy (komfort)
  }
}

// formatTemperature moved to display_config.h

String shortenDescription(String description) {
  // DEBUG - wypisz oryginalny opis w Serial
  Serial.println("Description API: '" + description + "'");
  
  String polishDescription = description;
  
  // Tłumaczenie angielskich opisów z OpenWeatherMap API na polski
  
  // === THUNDERSTORM (Grupa 2xx) ===
  if (description.indexOf("thunderstorm with heavy rain") >= 0) {
    polishDescription = "Burza z ulewa";
  } else if (description.indexOf("thunderstorm with rain") >= 0) {
    polishDescription = "Burza z deszczem";
  } else if (description.indexOf("thunderstorm with light rain") >= 0) {
    polishDescription = "Burza z mzawka";
  } else if (description.indexOf("thunderstorm with drizzle") >= 0) {
    polishDescription = "Burza z mzawka";
  } else if (description.indexOf("heavy thunderstorm") >= 0) {
    polishDescription = "Silna burza";
  } else if (description.indexOf("ragged thunderstorm") >= 0) {
    polishDescription = "Burza lokalna";
  } else if (description.indexOf("light thunderstorm") >= 0) {
    polishDescription = "Slaba burza";
  } else if (description.indexOf("thunderstorm") >= 0) {
    polishDescription = "Burza";
    
  // === DRIZZLE (Grupa 3xx) ===
  } else if (description.indexOf("heavy intensity drizzle") >= 0) {
    polishDescription = "Silna mzawka";
  } else if (description.indexOf("light intensity drizzle") >= 0) {
    polishDescription = "Lekka mzawka";
  } else if (description.indexOf("drizzle rain") >= 0) {
    polishDescription = "Mzawka";
  } else if (description.indexOf("shower drizzle") >= 0) {
    polishDescription = "Przelotna mzawka";
  } else if (description.indexOf("drizzle") >= 0) {
    polishDescription = "Mzawka";
    
  // === RAIN (Grupa 5xx) ===
  } else if (description.indexOf("extreme rain") >= 0) {
    polishDescription = "Ekstremalny deszcz";
  } else if (description.indexOf("very heavy rain") >= 0) {
    polishDescription = "Bardzo silny deszcz";
  } else if (description.indexOf("heavy intensity rain") >= 0) {
    polishDescription = "Ulewny deszcz";
  } else if (description.indexOf("heavy rain") >= 0) {
    polishDescription = "Silny deszcz";
  } else if (description.indexOf("moderate rain") >= 0) {
    polishDescription = "Umiarkowany deszcz";
  } else if (description.indexOf("light rain") >= 0) {
    polishDescription = "Slaby deszcz";
  } else if (description.indexOf("freezing rain") >= 0) {
    polishDescription = "Marznacy deszcz";
  } else if (description.indexOf("heavy intensity shower rain") >= 0) {
    polishDescription = "Silne opady";
  } else if (description.indexOf("ragged shower rain") >= 0) {
    polishDescription = "Lokalne opady";
  } else if (description.indexOf("shower rain") >= 0) {
    polishDescription = "Przelotne opady";
  } else if (description.indexOf("rain") >= 0) {
    polishDescription = "Deszcz";
    
  // === SNOW (Grupa 6xx) ===
  } else if (description.indexOf("heavy shower snow") >= 0) {
    polishDescription = "Sniezyca";
  } else if (description.indexOf("shower snow") >= 0) {
    polishDescription = "Przelotny snieg";
  } else if (description.indexOf("light shower snow") >= 0) {
    polishDescription = "Lekki snieg";
  } else if (description.indexOf("rain and snow") >= 0) {
    polishDescription = "Deszcz ze sniegiem";
  } else if (description.indexOf("light rain and snow") >= 0) {
    polishDescription = "Slaby snieg z deszczem";
  } else if (description.indexOf("shower sleet") >= 0) {
    polishDescription = "Deszcz ze sniegiem";
  } else if (description.indexOf("light shower sleet") >= 0) {
    polishDescription = "Lekka krupa";
  } else if (description.indexOf("sleet") >= 0) {
    polishDescription = "Krupa sniezna";
  } else if (description.indexOf("heavy snow") >= 0) {
    polishDescription = "Obfity snieg";
  } else if (description.indexOf("light snow") >= 0) {
    polishDescription = "Lekki snieg";
  } else if (description.indexOf("snow") >= 0) {
    polishDescription = "Snieg";
    
  // === ATMOSPHERE (Grupa 7xx) ===
  } else if (description.indexOf("sand/dust whirls") >= 0) {
    polishDescription = "Wiry piaskowe";
  } else if (description.indexOf("volcanic ash") >= 0) {
    polishDescription = "Popiol wulkaniczny";
  } else if (description.indexOf("squalls") >= 0) {
    polishDescription = "Szkwaly";
  } else if (description.indexOf("tornado") >= 0) {
    polishDescription = "Tornado";
  } else if (description.indexOf("mist") >= 0) {
    polishDescription = "Mgla";
  } else if (description.indexOf("smoke") >= 0) {
    polishDescription = "Dym";
  } else if (description.indexOf("haze") >= 0) {
    polishDescription = "Zamglenie";
  } else if (description.indexOf("dust") >= 0) {
    polishDescription = "Pyl";
  } else if (description.indexOf("fog") >= 0) {
    polishDescription = "Mgla";
  } else if (description.indexOf("sand") >= 0) {
    polishDescription = "Piasek";
  } else if (description.indexOf("ash") >= 0) {
    polishDescription = "Popiol";
    
  // === CLEAR (Grupa 800) ===
  } else if (description.indexOf("clear sky") >= 0) {
    polishDescription = "Bezchmurnie";
    
  // === CLOUDS (Grupa 80x) ===
  } else if (description.indexOf("overcast clouds") >= 0) {
    polishDescription = "Zachmurzenie calkowite";
  } else if (description.indexOf("broken clouds") >= 0) {
    polishDescription = "Duze zachmurzenie";
  } else if (description.indexOf("scattered clouds") >= 0) {
    polishDescription = "Umiarkowane zachmurzenie";
  } else if (description.indexOf("few clouds") >= 0) {
    polishDescription = "Male zachmurzenie";
  }
  
  // Nie skracamy opisu - zostanie obsłużony przez mniejszą czcionkę w displayWeather()
  // Usunięto logikę skracania z kropkami
  
  Serial.println("Polish text: '" + polishDescription + "'");
  return polishDescription;
}

void displayWeather(TFT_eSPI& tft) {
  if (!weather.isValid) return;
  
  // Sprawdź czy dane pogodowe się zmieniły
  if (!hasWeatherChanged()) {
    return; // Nie ma zmian - nie rysuj ponownie!
  }
  
  Serial.println("Weather data changed - redrawing display");
  
  // DEBUG: Wyświetl dodatkowe dane o opadach
  if (weather.rainLastHour > 0) {
    Serial.println("DESZCZ: " + String(weather.rainLastHour) + "mm/h");
  }
  if (weather.snowLastHour > 0) {
    Serial.println("ŚNIEG: " + String(weather.snowLastHour) + "mm/h");  
  }
  Serial.println("ZACHMURZENIE: " + String(weather.cloudiness) + "%");
  
  // Pozycja pogody - GÓRA na całej szerokości
  uint8_t x = WEATHER_AREA_X;
  uint8_t y = WEATHER_AREA_Y;
  
  // Wyczyść CAŁY obszar pogody gdy są zmiany
  tft.fillRect(x, y, WEATHER_AREA_WIDTH, WEATHER_AREA_HEIGHT, COLOR_BACKGROUND);
  
  // Ustawienia tekstu
  tft.setTextSize(FONT_SIZE_LARGE);
  tft.setTextDatum(TL_DATUM);
  
  // Ikona pogody
  drawWeatherIcon(tft, x + ICON_X_OFFSET, y + ICON_Y_OFFSET, weather.description, weather.icon);
  
  // Temperatura z odczuwalną w nawiasie - wyczyść całą linię
  tft.fillRect(x + TEMP_X_OFFSET, y + TEMP_Y_OFFSET, 200, 25, COLOR_BACKGROUND);
  
  // Dynamiczny kolor temperatury - niebieski dla mrozu
  uint16_t tempColor;
  if (weather.temperature < 0.0 || weather.feelsLike < 0.0) {
    tempColor = COLOR_TEMPERATURE_COLD;  // Niebieski dla temp < 0°C
    Serial.println("Uzywam niebieskiego koloru dla temperatury ponizej zera: " + String(weather.temperature, 1) + "°C");
  } else {
    tempColor = COLOR_TEMPERATURE;       // Pomarańczowy dla temp ≥ 0°C
  }
  tft.setTextColor(tempColor, COLOR_BACKGROUND);
  
  // Format temperatury bez znaku 'C (usunięty na żądanie użytkownika)
  String tempStr = formatTemperature(weather.temperature, 1) + "(" + formatTemperature(weather.feelsLike, 1) + ")";
  
  tft.setTextSize(FONT_SIZE_LARGE);   // Zawsze duża czcionka
  tft.drawString(tempStr, x + TEMP_X_OFFSET, y + TEMP_Y_OFFSET);
  
  // Przywróć normalną wielkość dla reszty elementów
  tft.setTextSize(FONT_SIZE_LARGE);
  
  // Opis pogody - wyczyść całą linię
  tft.fillRect(x + DESC_X_OFFSET, y + DESC_Y_OFFSET, 250, 25, COLOR_BACKGROUND);
  String fullDescription = shortenDescription(weather.description);
  
  // Sprawdź czy opis jest długi (>15 znaków) - jeśli tak, użyj mniejszej czcionki
  bool isLongDescription = fullDescription.length() > 15;
  
  // Specjalne kolory dla różnych opisów pogody (polskie nazwy)
  if (fullDescription.indexOf("Burza") >= 0) {
    tft.setTextColor(COLOR_DESCRIPTION_STORM, COLOR_BACKGROUND);  // Ciemny czerwony dla burzy
  } else if (fullDescription == "Bezchmurnie") {
    tft.setTextColor(COLOR_DESCRIPTION_SUNNY, COLOR_BACKGROUND);  // Żółty dla słońca
  } else if (fullDescription == "Mgla" || fullDescription == "Zamglenie" || fullDescription == "Dym") {
    tft.setTextColor(COLOR_DESCRIPTION_FOG, COLOR_BACKGROUND);    // Biały dla mgły
  } else {
    tft.setTextColor(COLOR_DESCRIPTION, COLOR_BACKGROUND);        // Cyan dla reszty
  }
  
  // Ustaw mniejszą czcionkę dla długich opisów
  if (isLongDescription) {
    tft.setTextSize(FONT_SIZE_LARGE - 1);  // O 1 mniejsza niż standardowa
  }
  
  tft.drawString(fullDescription, x + DESC_X_OFFSET, y + DESC_Y_OFFSET);
  
  // Przywróć standardową czcionkę
  tft.setTextSize(FONT_SIZE_LARGE);
  
  // Wilgotność - kolorowa na podstawie wartości
  tft.fillRect(x + HUMIDITY_X_OFFSET, y + HUMIDITY_Y_OFFSET, 120, 25, COLOR_BACKGROUND);
  uint16_t humidityColor = getHumidityColor(weather.humidity);
  tft.setTextColor(humidityColor, COLOR_BACKGROUND);
  String humStr = "Wilg:" + String(weather.humidity, 0) + "%";
  tft.drawString(humStr, x + HUMIDITY_X_OFFSET, y + HUMIDITY_Y_OFFSET);
  
  // Wyświetl prawdopodobieństwo opadów z prognozy (pierwszy element)
  uint8_t precipChance = 0;
  if (forecast.isValid && forecast.count > 0) {
    precipChance = forecast.items[0].precipitationChance; // Najbliższa prognoza
  }
// Użyj operatora trójargumentowego dla koloru
uint16_t precipColor = (precipChance == 0) ? TFT_WHITE : TFT_BLUE;

// Użyj operatora trójargumentowego dla etykiety (zachowując Twoją logikę oszczędzania miejsca)
String precipLabel = (precipChance == 100) ? "Op.:" : "Opad:";

// Zbuduj string końcowy
String precipStr = precipLabel + String(precipChance) + "%";
  
  
  tft.setTextColor(precipColor, COLOR_BACKGROUND);
  tft.drawString(precipStr, x + HUMIDITY_X_OFFSET + 160, y + HUMIDITY_Y_OFFSET);
  
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
  
  // Dodaj cienką białą linię oddzielającą sekcję pogody od czasu
  tft.drawLine(0, 180, tft.width(), 180, TFT_WHITE);
}