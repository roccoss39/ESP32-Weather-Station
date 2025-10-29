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
  String description = "";
  float humidity = 0;
  float windSpeed = 0;
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
      weather.humidity = doc["main"]["humidity"];
      weather.description = doc["weather"][0]["description"].as<String>();
      weather.windSpeed = doc["wind"]["speed"];
      weather.isValid = true;
      weather.lastUpdate = millis();
      
      Serial.println("Weather OK: " + String(weather.temperature) + "°C");
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
  
  condition.toLowerCase();
  
  if (condition.indexOf("słon") >= 0 || condition.indexOf("jas") >= 0) {
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
  else if (condition.indexOf("chmur") >= 0 || condition.indexOf("pochmur") >= 0) {
    // Chmura - białe/szare kółka
    tft.fillCircle(x + 15, y + 30, 12, TFT_LIGHTGREY);
    tft.fillCircle(x + 25, y + 25, 15, TFT_WHITE);
    tft.fillCircle(x + 35, y + 30, 12, TFT_LIGHTGREY);
    tft.fillRect(x + 10, y + 35, 30, 8, TFT_WHITE);
  }
  else if (condition.indexOf("deszcz") >= 0 || condition.indexOf("opad") >= 0) {
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
  else if (condition.indexOf("śnieg") >= 0) {
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
  else if (condition.indexOf("mgła") >= 0 || condition.indexOf("zamgl") >= 0) {
    // Mgła - poziome linie
    for (int i = 0; i < 4; i++) {
      tft.drawLine(x + 5, y + 20 + i * 5, x + 45, y + 20 + i * 5, TFT_LIGHTGREY);
    }
  }
  else {
    // Domyślna ikona - znak zapytania
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("?", x + 20, y + 15);
  }
}

// ===== FUNKCJE WYŚWIETLANIA =====
void displayWeather() {
  if (!weather.isValid) return;
  
  // Pozycja pogody - prawa strona, lepsze rozmieszczenie
  int x = 180;
  int y = 5;
  
  // Wyczyść cały obszar pogody
  tft.fillRect(x, y, 140, 110, TFT_BLACK);
  
  // Ikona pogody (góra)
  drawWeatherIcon(x + 5, y, weather.description);
  
  // Ustawienia tekstu - jednolity rozmiar
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  
  // Temperatura (obok ikony)
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  String tempStr = String(weather.temperature, 1) + "'C";
  tft.drawString(tempStr, x + 60, y + 5);
  
  // Opis pogody (pod ikoną)
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(weather.description, x + 5, y + 55);
  
  // Wilgotność
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String humStr = "Wilg: " + String(weather.humidity, 0) + "%";
  tft.drawString(humStr, x + 5, y + 80);
  
  // Wiatr
  String windStr = "Wiatr: " + String(weather.windSpeed, 1) + "m/s";
  tft.drawString(windStr, x + 5, y + 105);
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
    // Wyczyść obszar czasu (lewa strona ekranu)
    tft.fillRect(5, 5, 170, 120, TFT_BLACK);

    // ===== SEKCJA CZASU =====
    tft.setTextDatum(TL_DATUM); // Top Left alignment
    tft.setTextSize(2); // Jednolity rozmiar dla wszystkiego
    
    // Zegar na górze
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(timeStr, 10, 15);
    
    // Data pod zegarem
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(dateStr, 10, 40);
    
    // Dzień tygodnia
    char dayStr[20];
    strftime(dayStr, sizeof(dayStr), "%A", &timeinfo);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(String(dayStr), 10, 65);
    
    // Status WiFi
    if (WiFi.status() == WL_CONNECTED) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString("WiFi: OK", 10, 90);
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("WiFi: ERROR", 10, 90);
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

  delay(1000); // Aktualizuj co sekundę
}