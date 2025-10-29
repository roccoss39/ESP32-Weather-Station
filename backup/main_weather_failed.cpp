#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ===== KONFIGURACJA =====
const char* ssid = "zero";
const char* password = "Qweqweqwe1";  // Zmień na swoje hasło WiFi
const char* weatherApiKey = "ac44d6e8539af12c769627cbdfbbbe56";  // Klucz OpenWeatherMap
const char* city = "Szczecin";
const char* country = "PL";

// ===== OBIEKTY =====
TFT_eSPI tft = TFT_eSPI();

// ===== ZMIENNE GLOBALNE =====
struct tm timeinfo;
String lastTimeString = "";
String lastDateString = "";

// Zmienne pogodowe
struct {
  float temperature = 0;
  String description = "";
  float humidity = 0;
  float windSpeed = 0;
  int windDirection = 0;
  bool isValid = false;
  unsigned long lastUpdate = 0;
} weather;

unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_INTERVAL = 600000; // 10 minut

// ===== FUNKCJE POGODOWE =====
bool updateWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected - cannot get weather");
    return false;
  }

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
               String(city) + "," + String(country) + 
               "&appid=" + String(weatherApiKey) + 
               "&units=metric&lang=pl";
  
  Serial.println("Getting weather from: " + url);
  
  http.begin(url);
  http.setTimeout(10000); // 10 sekund timeout
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Weather API response received");
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.println("JSON parse error: " + String(error.c_str()));
      http.end();
      return false;
    }
    
    // Pobierz dane pogodowe
    weather.temperature = doc["main"]["temp"];
    weather.humidity = doc["main"]["humidity"];
    weather.description = doc["weather"][0]["description"].as<String>();
    weather.windSpeed = doc["wind"]["speed"];
    weather.windDirection = doc["wind"]["deg"];
    weather.isValid = true;
    weather.lastUpdate = millis();
    
    Serial.println("Weather updated: " + String(weather.temperature) + "°C, " + weather.description);
    
    http.end();
    return true;
  } else {
    Serial.println("HTTP error: " + String(httpCode));
    http.end();
    return false;
  }
}

String getWindDirection(int degrees) {
  if (degrees >= 337 || degrees < 23) return "N";
  else if (degrees < 67) return "NE";
  else if (degrees < 112) return "E";
  else if (degrees < 157) return "SE";
  else if (degrees < 202) return "S";
  else if (degrees < 247) return "SW";
  else if (degrees < 292) return "W";
  else return "NW";
}

// ===== FUNKCJE WYŚWIETLACZA =====
void displayTime() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  
  // Formatuj czas
  char timeStr[10];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  String currentTimeString = String(timeStr);
  
  // Formatuj datę
  char dateStr[15];
  strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);
  String currentDateString = String(dateStr);
  
  // Aktualizuj tylko jeśli czas się zmienił
  if (currentTimeString != lastTimeString) {
    // Wyczyść poprzedni czas
    if (lastTimeString != "") {
      tft.setTextSize(3);
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString(lastTimeString, 10, 10);
    }
    
    // Wyświetl nowy czas
    tft.setTextSize(3);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(currentTimeString, 10, 10);
    lastTimeString = currentTimeString;
  }
  
  // Aktualizuj datę
  if (currentDateString != lastDateString) {
    // Wyczyść poprzednią datę
    if (lastDateString != "") {
      tft.setTextSize(2);
      tft.setTextColor(TFT_BLACK, TFT_BLACK);
      tft.drawString(lastDateString, 10, 50);
    }
    
    // Wyświetl nową datę
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(currentDateString, 10, 50);
    lastDateString = currentDateString;
  }
}

void displayWeather() {
  if (!weather.isValid) return;
  
  // Obszar pogody - prawa strona ekranu
  int x = 180;
  int y = 10;
  
  // Wyczyść obszar pogody
  tft.fillRect(x, y, 140, 120, TFT_BLACK);
  
  // Temperatura - główna informacja
  tft.setTextSize(2);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  String tempStr = String(weather.temperature, 1) + "'C";
  tft.drawString(tempStr, x, y);
  
  // Opis pogody
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(weather.description, x, y + 25);
  
  // Wilgotność
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String humStr = "Wilg: " + String(weather.humidity, 0) + "%";
  tft.drawString(humStr, x, y + 40);
  
  // Wiatr
  String windStr = "Wiatr: " + String(weather.windSpeed, 1) + "m/s";
  tft.drawString(windStr, x, y + 55);
  
  // Kierunek wiatru
  String dirStr = "Kier: " + getWindDirection(weather.windDirection);
  tft.drawString(dirStr, x, y + 70);
  
  // Czas ostatniej aktualizacji
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  unsigned long minutesAgo = (millis() - weather.lastUpdate) / 60000;
  String updateStr = "Akt: " + String(minutesAgo) + "min";
  tft.drawString(updateStr, x, y + 85);
}

void displayWeatherError() {
  int x = 180;
  int y = 10;
  
  tft.fillRect(x, y, 140, 40, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("Blad pogody", x, y);
  tft.drawString("Sprawdz WiFi", x, y + 15);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Weather Station ===");
  
  // Inicjalizacja TFT - zachowaj działającą konfigurację!
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // Wyświetl status ładowania
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Laczenie WiFi...", 10, 100);
  
  // Połącz z WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.println("IP: " + WiFi.localIP().toString());
    
    // Konfiguruj czas
    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
    Serial.println("Time sync started...");
    
    // Wyczyść ekran
    tft.fillScreen(TFT_BLACK);
    
    // Pierwsze pobranie pogody
    Serial.println("Getting initial weather...");
    if (updateWeather()) {
      Serial.println("Initial weather loaded successfully");
    } else {
      Serial.println("Failed to load initial weather");
    }
    
  } else {
    Serial.println("\nWiFi connection failed!");
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("WiFi ERROR!", 10, 100);
    delay(5000);
    tft.fillScreen(TFT_BLACK);
  }
  
  Serial.println("Setup completed");
}

// ===== LOOP =====
void loop() {
  unsigned long currentTime = millis();
  
  // Wyświetl czas (zawsze)
  displayTime();
  
  // Aktualizuj pogodę co 10 minut
  if (currentTime - lastWeatherUpdate >= WEATHER_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Updating weather...");
      if (updateWeather()) {
        displayWeather();
      } else {
        displayWeatherError();
      }
    } else {
      Serial.println("WiFi disconnected, cannot update weather");
      displayWeatherError();
    }
    lastWeatherUpdate = currentTime;
  }
  
  // Wyświetl pogodę jeśli są dostępne dane
  if (weather.isValid) {
    displayWeather();
  }
  
  delay(1000); // Aktualizuj co sekundę
}