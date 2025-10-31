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
  } else if (humidity > 85.0) {
    Serial.println("Humidity: " + String(humidity, 0) + "% - WET (brązowy)");
    return COLOR_HUMIDITY_WET;     // >85% - brązowy (bardzo wilgotno)
  } else if (humidity > 70.0) {
    Serial.println("Humidity: " + String(humidity, 0) + "% - HUMID (fioletowy)");
    return COLOR_HUMIDITY_HUMID;   // 70-85% - zolty (duszno)
  } else {
    Serial.println("Humidity: " + String(humidity, 0) + "% - COMFORT (bialy)");
    return COLOR_HUMIDITY_COMFORT; // 30-70% - bialy (komfort)
  }
}

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
  
  // Skróć polską nazwę jeśli jest za długa (max 15 znaków + kropka)
  if (polishDescription.length() > 15) {
    if (polishDescription == "Zachmurzenie calkowite") {
      polishDescription = "Calkowite zachm.";
    } else if (polishDescription == "Umiarkowane zachmurzenie") {
      polishDescription = "Umiarkowane zachm.";
    } else if (polishDescription == "Duze zachmurzenie") {
      polishDescription = "Duze zachm.";
    } else if (polishDescription == "Male zachmurzenie") {
      polishDescription = "Male zachm.";
    } else if (polishDescription == "Ekstremalny deszcz") {
      polishDescription = "Ekstremalny des.";
    } else if (polishDescription == "Bardzo silny deszcz") {
      polishDescription = "Bardzo silny d.";
    } else if (polishDescription == "Umiarkowany deszcz") {
      polishDescription = "Umiarkowany des.";
    } else if (polishDescription == "Marznacy deszcz") {
      polishDescription = "Marznacy des.";
    } else if (polishDescription == "Deszcz ze sniegiem") {
      polishDescription = "Deszcz+snieg";
    } else if (polishDescription == "Popiol wulkaniczny") {
      polishDescription = "Popiol wulk.";
    } else if (polishDescription == "Slaby snieg z deszczem") {
      polishDescription = "Slaby snieg+d.";
    } else if (polishDescription == "Przelotna mzawka") {
      polishDescription = "Przelotna mz.";
    } else if (polishDescription == "Przelotne opady") {
      polishDescription = "Przelotne op.";
    } else if (polishDescription == "Przelotny snieg") {
      polishDescription = "Przelotny sn.";
    } else {
      // Skróć do 15 znaków i dodaj kropkę
      polishDescription = polishDescription.substring(0, 15) + ".";
    }
  }
  
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
  
  // Dynamiczny kolor temperatury - niebieski dla mrozu
  uint16_t tempColor;
  if (weather.temperature < 0.0 || weather.feelsLike < 0.0) {
    tempColor = COLOR_TEMPERATURE_COLD;  // Niebieski dla temp < 0°C
    Serial.println("Uzywam niebieskiego koloru dla temperatury ponizej zera: " + String(weather.temperature, 1) + "°C");
  } else {
    tempColor = COLOR_TEMPERATURE;       // Pomarańczowy dla temp ≥ 0°C
  }
  tft.setTextColor(tempColor, COLOR_BACKGROUND);
  
  // Skrócony format dla bardzo niskich temperatur (bez 'C)
  String tempStr;
  if (weather.temperature <= -5.0 || weather.feelsLike <= -5.0) {
    tempStr = String(weather.temperature, 1) + "(" + String(weather.feelsLike, 1) + ")";  // Bez 'C
    Serial.println("Uzywam skroconego formatu dla niskiej temperatury: " + String(weather.temperature, 1) + "°C");
  } else {
    tempStr = String(weather.temperature, 1) + "'C(" + String(weather.feelsLike, 1) + "'C)";  // Z 'C
  }
  
  tft.setTextSize(FONT_SIZE_LARGE);   // Zawsze duża czcionka
  tft.drawString(tempStr, x + TEMP_X_OFFSET, y + TEMP_Y_OFFSET);
  
  // Przywróć normalną wielkość dla reszty elementów
  tft.setTextSize(FONT_SIZE_LARGE);
  
  // Opis pogody - wyczyść całą linię
  tft.fillRect(x + DESC_X_OFFSET, y + DESC_Y_OFFSET, 250, 25, COLOR_BACKGROUND);
  String shortDescription = shortenDescription(weather.description);
  
  // Specjalne kolory dla różnych opisów pogody (polskie nazwy)
  if (shortDescription.indexOf("Burza") >= 0) {
    tft.setTextColor(COLOR_DESCRIPTION_STORM, COLOR_BACKGROUND);  // Ciemny czerwony dla burzy
  } else if (shortDescription == "Bezchmurnie") {
    tft.setTextColor(COLOR_DESCRIPTION_SUNNY, COLOR_BACKGROUND);  // Żółty dla słońca
  } else if (shortDescription == "Mgla" || shortDescription == "Zamglenie" || shortDescription == "Dym") {
    tft.setTextColor(COLOR_DESCRIPTION_FOG, COLOR_BACKGROUND);    // Biały dla mgły
  } else {
    tft.setTextColor(COLOR_DESCRIPTION, COLOR_BACKGROUND);        // Cyan dla reszty
  }
  
  tft.drawString(shortDescription, x + DESC_X_OFFSET, y + DESC_Y_OFFSET);
  
  // Wilgotność - kolorowa na podstawie wartości
  tft.fillRect(x + HUMIDITY_X_OFFSET, y + HUMIDITY_Y_OFFSET, 120, 25, COLOR_BACKGROUND);
  uint16_t humidityColor = getHumidityColor(weather.humidity);
  tft.setTextColor(humidityColor, COLOR_BACKGROUND);
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