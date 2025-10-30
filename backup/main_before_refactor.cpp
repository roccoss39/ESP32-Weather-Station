#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- USTAWIENIA UŻYTKOWNIKA ---
const char* ssid = "zero";
const char* password = "Qweqweqwe1";  // Sprawdź swoje hasło WiFi
const char* weatherApiKey = "ac44d6e8539af12c769627cbdfbbbe56";  // Klucz OpenWeatherMap
const char* city = "Szczecin";  // Twoje miasto

const char* ntpServer = "pool.ntp.org";
const char* tzInfo = "CET-1CEST,M3.5.0/2,M10.5.0/3";

TFT_eSPI tft = TFT_eSPI();

// Zmienne czasowe
char timeStrPrev[9] = "        ";
char dateStrPrev[11] = "          ";

// Zmienne pogodowe
struct WeatherData {
  float temperature = 0;
  float feelsLike = 0;  // Temperatura odczuwalna
  String description = "";
  float humidity = 0;
  float windSpeed = 0;
  float pressure = 0;  // Ciśnienie w hPa
  String icon = "";
  bool isValid = false;
  unsigned long lastUpdate = 0;
};

WeatherData weather;
unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_INTERVAL = 600000; // 10 minut

// ===== FUNKCJA POGODOWA =====
bool getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
               String(city) + ",PL&appid=" + String(weatherApiKey) + 
               "&units=metric&lang=pl";
  
  Serial.println("Getting weather...");
  
  http.begin(url);
  http.setTimeout(5000); // 5 sekund timeout
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      weather.temperature = doc["main"]["temp"];
      weather.feelsLike = doc["main"]["feels_like"];  // Temperatura odczuwalna
      weather.humidity = doc["main"]["humidity"];
      weather.pressure = doc["main"]["pressure"];  // Ciśnienie w hPa
      weather.description = doc["weather"][0]["description"].as<String>();
      weather.windSpeed = doc["wind"]["speed"];
      
      // Dodaj kod ikony z API
      if (doc["weather"][0]["icon"]) {
        weather.icon = doc["weather"][0]["icon"].as<String>();
        Serial.println("Ikona API: '" + weather.icon + "'");
      }
      
      weather.isValid = true;
      weather.lastUpdate = millis();
      
      Serial.println("Weather OK: " + String(weather.temperature) + "°C, odczuwalna: " + String(weather.feelsLike) + "°C, ciśnienie: " + String(weather.pressure) + "hPa");
      http.end();
      return true;
    }
  }
  
  Serial.println("Weather failed: " + String(httpCode));
  http.end();
  return false;
}

// ===== FUNKCJE IKON POGODOWYCH =====
void drawWeatherIcon(int x, int y, String condition) {
  tft.fillRect(x, y, 50, 50, TFT_BLACK); // Wyczyść obszar ikony
  
  // Sprawdź również kod ikony z API
  String iconCode = weather.icon;
  Serial.println("Rysowanie ikony dla: opis='" + condition + "', kod='" + iconCode + "'");
  
  // Ikony na podstawie kodu API (bardziej precyzyjne)
  if (iconCode.indexOf("01") >= 0) { // 01d, 01n = clear sky
    // Słońce - czyste niebo
    // Słońce - żółte koło z promieniami
    tft.fillCircle(x + 25, y + 25, 15, TFT_YELLOW);
    for (int i = 0; i < 8; i++) {
      float angle = i * 45 * PI / 180;
      int x1 = x + 25 + cos(angle) * 20;
      int y1 = y + 25 + sin(angle) * 20;
      int x2 = x + 25 + cos(angle) * 25;
      int y2 = y + 25 + sin(angle) * 25;
      tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
    }
  }
  else if (iconCode.indexOf("02") >= 0 || iconCode.indexOf("03") >= 0 || iconCode.indexOf("04") >= 0 || 
           condition.indexOf("chmur") >= 0 || condition.indexOf("pochmur") >= 0) {
    // 02d/02n = few clouds, 03d/03n = scattered clouds, 04d/04n = broken clouds
    // Chmura - białe/szare kółka
    tft.fillCircle(x + 15, y + 30, 12, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 25, 15, TFT_WHITE);
    tft.fillCircle(x + 35, y + 30, 12, TFT_LIGHTGREY);
    tft.fillRect(x + 10, y + 35, 30, 8, TFT_WHITE);
  }
  else if (iconCode.indexOf("09") >= 0 || iconCode.indexOf("10") >= 0 || 
           condition.indexOf("deszcz") >= 0 || condition.indexOf("opad") >= 0) {
    // 09d/09n = shower rain, 10d/10n = rain
    // Chmura z deszczem
    tft.fillCircle(x + 15, y + 20, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 15, 12, TFT_WHITE);
    tft.fillCircle(x + 35, y + 20, 10, TFT_LIGHTGREY);
    tft.fillRect(x + 12, y + 25, 26, 6, TFT_WHITE);
    // Krople deszczu
    for (int i = 0; i < 4; i++) {
      tft.drawLine(x + 15 + i * 5, y + 32, x + 15 + i * 5, y + 40, TFT_CYAN);
    }
  }
  else if (iconCode.indexOf("13") >= 0 || condition.indexOf("śnieg") >= 0 || condition.indexOf("snieg") >= 0) {
    // 13d/13n = snow
    // Chmura ze śniegiem
    tft.fillCircle(x + 15, y + 20, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 15, 12, TFT_WHITE);
    tft.fillCircle(x + 35, y + 20, 10, TFT_LIGHTGREY);
    tft.fillRect(x + 12, y + 25, 26, 6, TFT_WHITE);
    // Płatki śniegu
    for (int i = 0; i < 3; i++) {
      int sx = x + 18 + i * 8;
      int sy = y + 35;
      tft.drawPixel(sx, sy, TFT_WHITE);
      tft.drawPixel(sx-1, sy, TFT_WHITE);
      tft.drawPixel(sx+1, sy, TFT_WHITE);
      tft.drawPixel(sx, sy-1, TFT_WHITE);
      tft.drawPixel(sx, sy+1, TFT_WHITE);
    }
  }
  else if (iconCode.indexOf("50") >= 0 || condition.indexOf("mgła") >= 0 || condition.indexOf("zamgl") >= 0 || condition.indexOf("mgla") >= 0) {
    // 50d/50n = mist/fog
    // Mgła - poziome linie
    for (int i = 0; i < 4; i++) {
      tft.drawLine(x + 5, y + 20 + i * 5, x + 45, y + 20 + i * 5, TFT_LIGHTGREY);
    }
  }
  else {
    // Domyślna ikona - znak zapytania
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.drawString("?", x + 20, y + 15);
  }
}

// ===== FUNKCJE WYŚWIETLANIA =====
void displayWeather() {
  if (!weather.isValid) return;
  
  // Pozycja pogody - GÓRA na całej szerokości
  int x = 5;
  int y = 5;
  
  // Wyczyść obszar pogody - cała szerokość ekranu
  tft.fillRect(x, y, 310, 140, TFT_BLACK);
  
  // Ikona pogody (góra)
  drawWeatherIcon(x + 5, y, weather.description);
  
  // Ustawienia tekstu - większy rozmiar
  tft.setTextSize(3);
  tft.setTextDatum(TL_DATUM);
  
  // Temperatura (obok ikony) z odczuwalną w nawiasie
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  String tempStr = String(weather.temperature, 1) + "'C (" + String(weather.feelsLike, 1) + "'C)";
  tft.drawString(tempStr, x + 60, y + 5);
  
  // Opis pogody (pod ikoną) - debug i skrócenie
  tft.setTextSize(3);  // Przywróć rozmiar czcionki
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String shortDescription = weather.description;
  
  // DEBUG - wypisz oryginalny opis w Serial
  Serial.println("Opis pogody ORYGINALNY: '" + weather.description + "'");
  
  // Skracanie POLSKICH opisów z API (&lang=pl)
  if (shortDescription.indexOf("zachmurzenie duże") >= 0) {
    shortDescription = "Duze zachmurzenie";
  } else if (shortDescription.indexOf("zachmurzenie małe") >= 0) {
    shortDescription = "Male zachmurzenie.";
  } else if (shortDescription.indexOf("zachmurzenie umiarkowane") >= 0) {
    shortDescription = "Umiark. zachmurzenie";
  } else if (shortDescription.indexOf("zachmurzenie") >= 0) {
    shortDescription = "Zachmurzurzenie";
  } else if (shortDescription.indexOf("pochmurnie") >= 0) {
    shortDescription = "Pochmurnie";
  } else if (shortDescription.indexOf("bezchmurnie") >= 0) {
    shortDescription = "Bezchmurnie";
  } else if (shortDescription.indexOf("słonecznie") >= 0) {
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
    // Jeśli nic nie pasuje, skróć do 11 znaków
    if (shortDescription.length() > 11) {
      shortDescription = shortDescription.substring(0, 11) + ".";
    }
  }
  
  Serial.println("Wyswietlany opis: '" + shortDescription + "'");
  
  tft.drawString(shortDescription, x + 5, y + 55);
  
  // Wilgotność
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String humStr = "Wilg: " + String(weather.humidity, 0) + "%";
  tft.drawString(humStr, x + 5, y + 85);
  
  // Wiatr (przelicz z m/s na km/h)
  float windKmh = weather.windSpeed * 3.6;
  String windStr = "Wiatr: " + String(windKmh, 1) + "km/h";
  tft.drawString(windStr, x + 5, y + 115);
  
  // Ciśnienie
  String pressureStr = "Cisnienie: " + String(weather.pressure, 0) + "hPa";
  tft.drawString(pressureStr, x + 170, y + 115);
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32 Weather Station ===");

  // --- Inicjalizacja TFT (ZACHOWUJĘ DZIAŁAJĄCĄ KONFIGURACJĘ) ---
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(3);

  tft.drawString("Laczenie WiFi...", tft.width() / 2, tft.height() / 2);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  // --- Łączenie z WiFi ---
  WiFi.begin(ssid, password);
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
  configTzTime(tzInfo, ntpServer);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    tft.fillScreen(TFT_RED);
    tft.drawString("Time Error!", tft.width() / 2, tft.height() / 2);
  } else {
    Serial.println("Time synchronized");
  }
  
  tft.fillScreen(TFT_BLACK); // Wyczyść ekran
  
  // Pierwsze pobranie pogody (nie blokujące)
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Getting initial weather...");
    getWeather();
  }
  
  Serial.println("Setup completed");
}

void loop() {
  struct tm timeinfo;

  // --- WYŚWIETLANIE CZASU (jak wcześniej) ---
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error reading time");
    delay(1000);
    return;
  }

  // Formatuj czas
  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  // Formatuj datę
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);

  // Rysuj tylko jeśli czas się zmienił
  if (strcmp(timeStr, timeStrPrev) != 0) {
    // Wyczyść obszar czasu (POD POGODĄ na całej szerokości)
    tft.fillRect(5, 150, 310, 85, TFT_BLACK);

    // ===== SEKCJA CZASU - POD POGODĄ =====
    tft.setTextDatum(TL_DATUM); // Top Left alignment
    tft.setTextSize(2); // Mniejsza czcionka
    
    // Czas, data i dzień w jednym wierszu
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(timeStr, 10, 155); // Czas po lewej
    
    // Data po prawej od czasu
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(dateStr, 130, 155); // Data obok czasu
    
    // Dzień tygodnia po polsku - w drugim wierszu
    char dayStr[20];
    strftime(dayStr, sizeof(dayStr), "%w", &timeinfo); // Pobierz numer dnia (0=niedziela)
    String polishDay;
    int dayNum = atoi(dayStr);
    switch(dayNum) {
      case 0: polishDay = "Niedziela"; break;
      case 1: polishDay = "Poniedzialek"; break;
      case 2: polishDay = "Wtorek"; break;
      case 3: polishDay = "Sroda"; break;
      case 4: polishDay = "Czwartek"; break;
      case 5: polishDay = "Piatek"; break;
      case 6: polishDay = "Sobota"; break;
      default: polishDay = "Nieznany"; break;
    }
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(polishDay, 10, 180); // Dzień po lewej
    
    // Status WiFi w tej samej linii po prawej
    if (WiFi.status() == WL_CONNECTED) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString("WiFi: OK", 180, 180); // WiFi po prawej stronie
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("WiFi: BLAD", 180, 180);
    }

    strcpy(timeStrPrev, timeStr);
    strcpy(dateStrPrev, dateStr);

    Serial.print("Time: ");
    Serial.print(dateStr);
    Serial.print(" ");
    Serial.println(timeStr);
  }

  // --- AKTUALIZACJA POGODY (co 10 minut) ---
  unsigned long currentTime = millis();
  if (currentTime - lastWeatherUpdate >= WEATHER_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Updating weather...");
      getWeather();
    }
    lastWeatherUpdate = currentTime;
  }

  // --- WYŚWIETLANIE POGODY ---
  if (weather.isValid) {
    displayWeather();
  }

  // Prawa kolumna usunięta - więcej miejsca na lewą stronę

  delay(1000); // Aktualizuj co sekundę
}