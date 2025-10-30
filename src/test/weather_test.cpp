#include "weather/weather_data.h"
#include "weather/forecast_data.h"
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
  // Test 1: MIESZANE TEMPERATURY - dwucyfrowa ujemna + dodatnia
  {-12.5, -15.8, "śnieg", 85, 18.2, 995, "13d", "MIESZANE 1"},
  
  // Test 2: EKSTREMALNE PRZECIWNOŚCI - bardzo zimno vs ciepło  
  {-10.0, 10.0, "przejściowe", 70, 25.3, 1008, "04d", "MIESZANE 2"},
  
  // Test 3: ORYGINALNE SŁONECZNIE (przywrócone)
  {23.5, 25.1, "bezchmurnie", 45, 2.1, 1025, "01d", "SŁONECZNIE"},
  
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
  {38.7, 42.1, "słonecznie", 25, 5.4, 1012, "01d", "UPAŁ"},
  
  // === TESTY SPECJALNE DLA PROGNOZY ===
  
  // Test 13: Arktyczne mrozy (dwucyfrowe minusy)
  {-15.2, -22.8, "śnieg", 92, 28.7, 988, "13d", "ARKTYKA"},
  
  // Test 14: Skrajnie zimno (jeszcze zimniej)
  {-25.8, -32.1, "zamieć", 85, 45.3, 965, "13d", "SYBERIA"},
  
  // Test 15: Przejściowy (zero stopni)
  {0.2, -2.1, "śnieg z deszczem", 78, 15.6, 1002, "13d", "PRZEJŚCIE"},
  
  // Test 16: Wietrzny letni dzień
  {32.1, 36.8, "słonecznie", 40, 38.9, 1018, "01d", "WIETRZNIE"},
  
  // Test 17: Tropikalna burza
  {28.5, 31.2, "burza z deszczem", 95, 42.7, 978, "11d", "TROPIKI"},
  
  // === TESTY SPECJALNE IKON ===
  
  // Test 18: Test wszystkich ikon słonecznych
  {24.8, 26.1, "bezchmurnie", 50, 8.2, 1020, "01d", "SŁOŃCE TEST"},
  
  // Test 19: Test wszystkich ikon chmur
  {18.5, 19.2, "zachmurzenie", 70, 12.4, 1010, "03d", "CHMURY TEST"},
  
  // Test 20: Test wszystkich ikon opadów
  {15.8, 14.1, "deszcz", 85, 18.9, 1005, "10d", "OPADY TEST"}
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

// Funkcja ładująca testy prognozy
void loadTestForecast(int testIndex) {
  if (testIndex < 0 || testIndex >= TEST_DATA_SIZE) return;
  
  Serial.println("=== GENEROWANIE TESTOWEJ PROGNOZY ===");
  
  // Wyczyść poprzednie dane prognozy
  forecast.count = 0;
  forecast.isValid = false;
  
  // Generuj 5 prognoz na podstawie aktualnego testu
  TestWeatherData& baseTest = testData[testIndex];
  
  for (int i = 0; i < 5; i++) {
    // Godziny co 3h od 15:00
    int hour = 15 + (i * 3);
    if (hour >= 24) hour -= 24;
    
    char timeStr[6];
    sprintf(timeStr, "%02d:00", hour);
    forecast.items[i].time = String(timeStr);
    
    // Temperatura z wariacjami
    float tempVariation;
    if (testIndex == 0) { // Test 1: MIESZANE TEMPERATURY - od -15°C do +5°C
      float mixedTemps[] = {-15.0, -8.0, -2.0, 3.0, 5.0}; // Test formatowania mieszanego
      forecast.items[i].temperature = mixedTemps[i];
    } else if (testIndex == 1) { // Test 2: DRUGI TEST MIESZANY  
      float mixedTemps2[] = {-12.0, -6.0, 0.0, 8.0, 15.0}; // Kolejny test mieszany
      forecast.items[i].temperature = mixedTemps2[i];
    } else if (testIndex == 2) { // Test 3: TRZECI TEST MIESZANY
      float mixedTemps3[] = {-20.0, -3.0, 4.0, 12.0, 18.0}; // Jeszcze jeden test mieszany  
      forecast.items[i].temperature = mixedTemps3[i];
    } else {
      // Standardowe wariacje (+/- 3°C od bazowej)
      tempVariation = (i - 2) * 1.5; // -3, -1.5, 0, +1.5, +3
      forecast.items[i].temperature = baseTest.temperature + tempVariation;
    }
    
    // Wiatr z wariacjami
    float windVariation = (i * 0.5); // 0, 0.5, 1.0, 1.5, 2.0 m/s
    forecast.items[i].windSpeed = baseTest.windSpeed + windVariation;
    
    // Ikony - różne warianty w zależności od typu testu
    String icons[5];
    String descriptions[5];
    
    if (testIndex == 6) { // Test 7: BURZA - test ikony burzy
      String stormIcons[] = {"11d", "10d", "09d", "04d", "11d"};
      String stormDescs[] = {"burza z deszczem", "deszcz", "silny deszcz", "duże chmury", "burza"};
      for (int j = 0; j < 5; j++) { icons[j] = stormIcons[j]; descriptions[j] = stormDescs[j]; }
    } 
    else if (testIndex == 8) { // Test 9: MGŁA - test ikony mgły  
      String fogIcons[] = {"50d", "04d", "03d", "50d", "50d"};
      String fogDescs[] = {"mgła", "duże chmury", "umiarkowane chmury", "mgła", "mgła"};
      for (int j = 0; j < 5; j++) { icons[j] = fogIcons[j]; descriptions[j] = fogDescs[j]; }
    }
    else if (testIndex == 16) { // Test 17: TROPIKI - mix burzowy
      String tropicIcons[] = {"11d", "09d", "10d", "11d", "04d"};  
      String tropicDescs[] = {"burza", "deszcz ulewny", "deszcz", "burza z piorunami", "chmury burzowe"};
      for (int j = 0; j < 5; j++) { icons[j] = tropicIcons[j]; descriptions[j] = tropicDescs[j]; }
    }
    else if (testIndex == 17) { // Test 18: SŁOŃCE TEST - wszystkie słoneczne
      String sunIcons[] = {"01d", "01n", "02d", "02n", "01d"};
      String sunDescs[] = {"bezchmurnie dzień", "bezchmurnie noc", "lekkie chmury dzień", "lekkie chmury noc", "słonecznie"};
      for (int j = 0; j < 5; j++) { icons[j] = sunIcons[j]; descriptions[j] = sunDescs[j]; }
    }
    else if (testIndex == 18) { // Test 19: CHMURY TEST - gradacja chmur
      String cloudIcons[] = {"02d", "03d", "04d", "04n", "03n"};
      String cloudDescs[] = {"lekkie chmury", "umiarkowane chmury", "duże chmury", "chmury noc", "umiarkowane noc"};
      for (int j = 0; j < 5; j++) { icons[j] = cloudIcons[j]; descriptions[j] = cloudDescs[j]; }
    }
    else if (testIndex == 19) { // Test 20: OPADY TEST - wszystkie rodzaje opadów
      String rainIcons[] = {"09d", "10d", "11d", "13d", "50d"};
      String rainDescs[] = {"deszcz ulewny", "deszcz", "burza", "śnieg", "mgła"};
      for (int j = 0; j < 5; j++) { icons[j] = rainIcons[j]; descriptions[j] = rainDescs[j]; }
    }
    else if (testIndex == 0) { // Test 1: MIESZANE TEMPERATURY - mix ikon dla różnych temp
      String mixedIcons[] = {"13d", "13d", "03d", "02d", "01d"}; // Śnieg → chmury → słońce
      String mixedDescs[] = {"śnieg", "śnieg lekki", "chmury", "lekkie chmury", "słonecznie"};
      for (int j = 0; j < 5; j++) { icons[j] = mixedIcons[j]; descriptions[j] = mixedDescs[j]; }
    }
    else if (testIndex == 1) { // Test 2: DRUGI MIESZANY - przejściowy
      String transIcons[] = {"13d", "10d", "03d", "02d", "01d"}; // Od śniegu do słońca
      String transDescs[] = {"śnieg", "deszcz ze śniegiem", "chmury", "częściowo słonecznie", "słonecznie"};  
      for (int j = 0; j < 5; j++) { icons[j] = transIcons[j]; descriptions[j] = transDescs[j]; }
    }
    else if (testIndex == 2) { // Test 3: TRZECI MIESZANY - ekstremalne przejście
      String extremeIcons[] = {"13d", "04d", "02d", "01d", "01d"}; // Mróz → ciepło
      String extremeDescs[] = {"zamieć", "chmury", "lekkie chmury", "słonecznie", "gorąco"};
      for (int j = 0; j < 5; j++) { icons[j] = extremeIcons[j]; descriptions[j] = extremeDescs[j]; }
    }
    else { // Standardowy cykl dla innych testów
      String defaultIcons[] = {baseTest.icon, "02d", "03d", "04d", "10d"};
      String defaultDescs[] = {baseTest.description, "lekkie chmury", "umiarkowane chmury", "duże chmury", "deszcz"};
      for (int j = 0; j < 5; j++) { icons[j] = defaultIcons[j]; descriptions[j] = defaultDescs[j]; }
    }
    
    forecast.items[i].icon = icons[i];
    forecast.items[i].description = descriptions[i];
    
    forecast.count++;
    
    Serial.println("Prognoza " + String(i+1) + ": " + forecast.items[i].time + 
                  " - " + String(forecast.items[i].temperature, 1) + "°C - " + 
                  String(forecast.items[i].windSpeed * 3.6, 1) + "km/h - " + 
                  forecast.items[i].icon);
  }
  
  forecast.isValid = true;
  forecast.lastUpdate = millis();
  
  Serial.println("Testowa prognoza załadowana (" + String(forecast.count) + " elementów)");
}

// Funkcja testowa do wywołania w loop()
void runWeatherTest() {
  unsigned long currentTime = millis();
  
  // Sprawdź czy czas na następny test
  if (currentTime - lastTestUpdate >= TEST_INTERVAL) {
    loadTestData(currentTestIndex);
    
    // Generuj też prognozę dla ekranu 2
    loadTestForecast(currentTestIndex);
    
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