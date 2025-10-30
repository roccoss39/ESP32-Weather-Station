#include "weather/weather_data.h"
#include "display/weather_display.h"
#include "display/time_display.h"
#include <Arduino.h>

// Test data - różne scenariusze pogodowe
struct TestWeatherData {
  float temperature;
  float feelsLike;
  String description;
  float humidity;
  float windSpeed;
  float pressure;
  String icon;
  String testName;
};

// Tablica testowych danych pogodowych
TestWeatherData testData[] = {
  // Test 1: Słonecznie
  {23.5, 25.1, "bezchmurnie", 45, 2.1, 1025, "01d", "SŁONECZNIE"},
  
  // Test 2: Lekkie zachmurzenie
  {21.2, 22.8, "zachmurzenie małe", 55, 3.5, 1020, "02d", "LEKKIE CHMURY"},
  
  // Test 3: Umiarkowane zachmurzenie
  {19.8, 18.9, "zachmurzenie umiarkowane", 65, 8.2, 1015, "03d", "UMIARKOWANE CHMURY"},
  
  // Test 4: Duże zachmurzenie
  {18.1, 16.5, "zachmurzenie duże", 75, 12.1, 1010, "04d", "DUŻE CHMURY"},
  
  // Test 5: Deszcz lekki
  {16.3, 14.2, "deszcz lekki", 85, 15.8, 1005, "10d", "LEKKI DESZCZ"},
  
  // Test 6: Deszcz silny + silny wiatr
  {14.7, 12.1, "deszcz silny", 90, 22.5, 995, "09d", "SILNY DESZCZ"},
  
  // Test 7: Burza + bardzo silny wiatr
  {12.8, 10.2, "burza z deszczem", 95, 28.3, 985, "11d", "BURZA"},
  
  // Test 8: Śnieg
  {-2.1, -4.8, "śnieg", 80, 18.7, 1000, "13d", "ŚNIEG"},
  
  // Test 9: Mgła
  {8.5, 6.9, "mgła", 98, 1.2, 1018, "50d", "MGŁA"},
  
  // Test 10: Bardzo wysokie ciśnienie
  {25.8, 28.2, "bezchmurnie", 35, 1.8, 1035, "01d", "WYSOKIE CIŚNIENIE"},
  
  // Test 11: Ekstremalne warunki
  {-8.5, -12.3, "zamieć śnieżna", 88, 35.2, 975, "13d", "EKSTREMALNE"},
  
  // Test 12: Bardzo gorąco
  {38.7, 42.1, "słonecznie", 25, 5.4, 1012, "01d", "UPAŁ"}
};

const int TEST_DATA_SIZE = sizeof(testData) / sizeof(testData[0]);
int currentTestIndex = 0;
unsigned long lastTestUpdate = 0;
const unsigned long TEST_INTERVAL = 4000; // 4 sekundy

// Funkcja ładująca dane testowe
void loadTestData(int index) {
  if (index < 0 || index >= TEST_DATA_SIZE) {
    Serial.println("ERROR: Test index out of range!");
    return;
  }
  
  TestWeatherData& test = testData[index];
  
  // Bezpieczne ładowanie danych
  weather.temperature = test.temperature;
  weather.feelsLike = test.feelsLike;
  weather.description = test.description;
  weather.humidity = constrain(test.humidity, 0, 100); // 0-100%
  weather.windSpeed = max(0.0f, test.windSpeed);       // >= 0
  weather.pressure = constrain(test.pressure, 900, 1100); // rozumny zakres
  weather.icon = test.icon;
  weather.isValid = true;
  weather.lastUpdate = millis();
  
  // Debug info
  Serial.println("=== TEST " + String(index + 1) + "/" + String(TEST_DATA_SIZE) + ": " + test.testName + " ===");
  Serial.println("Temp: " + String(weather.temperature, 1) + "°C (odcz: " + String(weather.feelsLike, 1) + "°C)");
  Serial.println("Opis: '" + weather.description + "'");
  Serial.println("Ikona: '" + weather.icon + "'");
  Serial.println("Wilg: " + String(weather.humidity, 0) + "%");
  Serial.println("Wiatr: " + String(weather.windSpeed * 3.6, 1) + " km/h");
  Serial.println("Ciśn: " + String(weather.pressure, 0) + " hPa");
  Serial.println();
}

// Funkcja testowa do wywołania w loop()
void runWeatherTest() {
  unsigned long currentTime = millis();
  
  // Sprawdź czy czas na następny test
  if (currentTime - lastTestUpdate >= TEST_INTERVAL) {
    loadTestData(currentTestIndex);
    
    // Przejdź do następnego testu (cyklicznie)
    currentTestIndex = (currentTestIndex + 1) % TEST_DATA_SIZE;
    lastTestUpdate = currentTime;
    
    Serial.println("Następny test za " + String(TEST_INTERVAL / 1000) + " sekund...");
    Serial.println("----------------------------------------");
  }
}

// Funkcja inicjalizująca test
void initWeatherTest() {
  Serial.println("=== WEATHER DISPLAY TEST STARTED ===");
  Serial.println("Testowanie " + String(TEST_DATA_SIZE) + " scenariuszy pogodowych");
  Serial.println("Zmiana co " + String(TEST_INTERVAL / 1000) + " sekund");
  Serial.println("==========================================");
  Serial.println();
  
  // Załaduj pierwszy test
  loadTestData(0);
  lastTestUpdate = millis();
}

// Funkcja resetująca test
void resetWeatherTest() {
  currentTestIndex = 0;
  lastTestUpdate = 0;
  Serial.println("=== TEST RESET ===");
  initWeatherTest();
}

// Funkcja sprawdzająca poprawność danych (bezpieczeństwo)
bool validateWeatherData() {
  if (!weather.isValid) {
    Serial.println("WARNING: Weather data marked as invalid!");
    return false;
  }
  
  if (weather.temperature < -50 || weather.temperature > 60) {
    Serial.println("ERROR: Temperature out of range: " + String(weather.temperature));
    return false;
  }
  
  if (weather.humidity < 0 || weather.humidity > 100) {
    Serial.println("ERROR: Humidity out of range: " + String(weather.humidity));
    return false;
  }
  
  if (weather.pressure < 900 || weather.pressure > 1100) {
    Serial.println("ERROR: Pressure out of range: " + String(weather.pressure));
    return false;
  }
  
  if (weather.windSpeed < 0) {
    Serial.println("ERROR: Negative wind speed: " + String(weather.windSpeed));
    return false;
  }
  
  return true;
}