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

// ===== FUNKCJE WYŚWIETLANIA =====
void displayWeather() {
  if (!weather.isValid) return;
  
  // Pozycja pogody - prawa strona
  int x = 200;
  int y = 10;
  
  // Temperatura
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  String tempStr = String(weather.temperature, 1) + "'C";
  tft.fillRect(x, y, 120, 20, TFT_BLACK); // Wyczyść obszar
  tft.drawString(tempStr, x, y);
  
  // Opis
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.fillRect(x, y + 25, 120, 10, TFT_BLACK);
  tft.drawString(weather.description, x, y + 25);
  
  // Wilgotność
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String humStr = "Wilg: " + String(weather.humidity, 0) + "%";
  tft.fillRect(x, y + 40, 120, 10, TFT_BLACK);
  tft.drawString(humStr, x, y + 40);
  
  // Wiatr
  String windStr = "Wiatr: " + String(weather.windSpeed, 1) + "m/s";
  tft.fillRect(x, y + 55, 120, 10, TFT_BLACK);
  tft.drawString(windStr, x, y + 55);
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
    // Wyczyść obszar czasu
    tft.fillRect(0, tft.height()/2 - 20, 180, 40, TFT_BLACK);

    // Narysuj czas
    tft.setTextSize(3);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(timeStr, 90, tft.height() / 2);

    // Narysuj datę
    tft.setTextSize(2);
    tft.drawString(dateStr, 90, tft.height() / 2 + 30);

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