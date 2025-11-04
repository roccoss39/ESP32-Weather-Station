#include "display/github_image.h"
#include "config/display_config.h"
#include <TJpg_Decoder.h>
#include <HTTPClient.h>

// --- ZMIENNE GLOBALNE ---
CurrentImageData currentImage;

// --- INCLUDE ULTIMATE NASA COLLECTION (401 obrazków) ---
#include "photo_display/esp32_nasa_ultimate.h"

// --- TEST CONFIG: 3 obrazki co 3 sekundy ---
const int TEST_IMAGE_COUNT = 3;              // Tylko 3 obrazki dla testów
const unsigned long TEST_CHANGE_INTERVAL = 3000;  // 3 sekundy

// Callback dla TJpg_Decoder (z photo_display)
bool tft_output_nasa(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  extern TFT_eSPI tft;
  if (y >= 240) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void initNASAImageSystem() {
  Serial.println("=== INICJALIZACJA NASA ULTIMATE SYSTEM ===");
  Serial.println("Calkowita kolekcja: " + String(num_nasa_images) + " obrazków");
  Serial.println("TEST MODE: " + String(TEST_IMAGE_COUNT) + " obrazki co " + String(TEST_CHANGE_INTERVAL/1000) + " sekund");
  
  // Zainicjalizuj pierwszy obraz (z pierwszych 3 dla testów)
  currentImage.imageNumber = 0; // Zacznij od pierwszego obrazka
  currentImage.url = String(nasa_ultimate_collection[currentImage.imageNumber].url);
  currentImage.title = String(nasa_ultimate_collection[currentImage.imageNumber].title);
  currentImage.date = String(nasa_ultimate_collection[currentImage.imageNumber].filename); // filename zamiast date
  currentImage.isValid = false;
  currentImage.lastUpdate = 0;
  
  Serial.println("NASA Ultimate System gotowy!");
  Serial.println("GitHub URLs: roccoss39.github.io/nasa.github.io-/");
}

bool getRandomNASAImage() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Brak WiFi - nie można pobrać zdjęcia NASA");
    return false;
  }
  
  // TEST MODE: Cyklicznie przez pierwsze 3 obrazki (0, 1, 2)
  static int testIndex = 0;
  currentImage.imageNumber = testIndex;
  testIndex = (testIndex + 1) % TEST_IMAGE_COUNT; // 0->1->2->0->1->2...
  
  currentImage.url = String(nasa_ultimate_collection[currentImage.imageNumber].url);
  currentImage.title = String(nasa_ultimate_collection[currentImage.imageNumber].title);
  currentImage.date = String(nasa_ultimate_collection[currentImage.imageNumber].filename); // filename as date
  currentImage.lastUpdate = millis();
  
  Serial.println("=== NASA Image " + String(currentImage.imageNumber + 1) + " ===");
  Serial.println("Tytuł: " + currentImage.title);
  Serial.println("URL: " + currentImage.url);
  
  currentImage.isValid = true;
  return true;
}

void displayGitHubImage(TFT_eSPI& tft) {
  Serial.println("=== EKRAN NASA ULTIMATE ===");
  
  // AUTO-ADVANCE: Zmiana obrazka co 3 sekundy
  static unsigned long lastImageChange = 0;
  static bool firstRun = true;
  
  if (firstRun || (millis() - lastImageChange >= TEST_CHANGE_INTERVAL)) {
    Serial.println("🔄 Zmiana obrazka NASA...");
    
    if (!getRandomNASAImage()) {
      // Nie udało się pobrać - pokaż błąd
      tft.fillScreen(COLOR_BACKGROUND);
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("BLAD NASA", tft.width() / 2, tft.height() / 2 - 20);
      tft.setTextSize(1);
      tft.drawString("Sprawdz polaczenie", tft.width() / 2, tft.height() / 2 + 10);
      return;
    }
    
    // Spróbuj załadować i wyświetlić nowy NASA obrazek
    if (downloadAndDisplayImage(tft, currentImage.imageNumber)) {
      Serial.println("✅ NASA obraz wyświetlony pomyślnie!");
      lastImageChange = millis();
      firstRun = false;
    } else {
      Serial.println("❌ Nie udało się wyświetlić NASA obrazka");
      
      // Pokaż błąd
      tft.fillScreen(COLOR_BACKGROUND);
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("BLAD POBIERANIA", tft.width() / 2, tft.height() / 2 - 20);
      tft.setTextSize(1);
      tft.drawString("NASA Connection Error", tft.width() / 2, tft.height() / 2 + 10);
      tft.drawString("GitHub connection failed", tft.width() / 2, tft.height() / 2 + 30);
    }
  }
}

bool downloadAndDisplayImage(TFT_eSPI& tft, int imageIndex) {
  if (imageIndex >= num_nasa_images || imageIndex < 0) return false;
  
  NASAImage selectedImage = nasa_ultimate_collection[imageIndex];
  
  Serial.println("=== NASA Image " + String(imageIndex + 1) + " ===");
  Serial.println("Title: " + String(selectedImage.title));
  Serial.println("Filename: " + String(selectedImage.filename));
  Serial.println("URL: " + String(selectedImage.url));
  
  // Pokaż loading screen (czysty)
  tft.fillScreen(TFT_BLACK);
  
  // HTTP download przez HTTPClient (prostsze niż WiFiClientSecure)
  HTTPClient http;
  http.begin(selectedImage.url);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("❌ HTTP Error: " + String(httpCode));
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Download Failed!", 10, 100);
    tft.drawString("HTTP: " + String(httpCode), 10, 120);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  Serial.println("File size: " + String(contentLength) + " bytes");
  
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.println("❌ No stream available");
    http.end();
    return false;
  }
  
  // Download to buffer
  uint8_t* buffer = (uint8_t*)malloc(contentLength);
  if (!buffer) {
    Serial.println("❌ Memory allocation failed");
    http.end();
    return false;
  }
  
  stream->readBytes(buffer, contentLength);
  
  // Clear and display image
  tft.fillScreen(TFT_BLACK);
  
  // Setup TJpgDecoder with RGB swap for correct colors
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);  // Fix purple/violet colors
  TJpgDec.setCallback(tft_output_nasa);
  
  int result = TJpgDec.drawJpg(0, 0, buffer, contentLength);
  
  if (result == 0) {
    // Dodaj tytuł i informacje (jak w photo_display)
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    
    String titleStr = String(selectedImage.title);
    if (titleStr.length() > 45) {
      titleStr = titleStr.substring(0, 42) + "...";
    }
    
    int titleWidth = tft.textWidth(titleStr);
    int titleX = (320 - titleWidth) / 2;
    if (titleX < 0) titleX = 5;
    
    tft.drawString(titleStr, titleX, 225);
    
    // Usuń progress info - tylko czysty obrazek
    
    Serial.println("✅ NASA image displayed successfully!");
    free(buffer);
    http.end();
    return true;
  } else {
    Serial.println("✗ JPEG decode error: " + String(result));
    free(buffer);
    http.end();
    return false;
  }
}