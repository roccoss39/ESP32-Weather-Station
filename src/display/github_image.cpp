#include "display/github_image.h"
#include "config/display_config.h"
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include "SPIFFS.h"

#define TEST_IMG 0

// --- ZMIENNE GLOBALNE ---
CurrentImageData currentImage;

// --- INCLUDE ULTIMATE NASA COLLECTION (1359 obrazków) ---
#include "photo_display/esp32_nasa_ultimate.h"

// --- RANDOM CONFIG: wszystkie obrazki ---
const unsigned long IMAGE_CHANGE_INTERVAL = 10000;  // 3 sekundy

// Callback dla TJpg_Decoder (z photo_display) - Z DEBUG
bool tft_output_nasa(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  extern TFT_eSPI tft;
  
  static int callbackCount = 0;
  callbackCount++;
  
  // Debug pierwszych 5 wywołań
  if (callbackCount <= 5) {
    Serial.printf("🎨 NASA Callback #%d: x=%d, y=%d, w=%d, h=%d\n", callbackCount, x, y, w, h);
  }
  
  if (y >= 240) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// Funkcja ładowania fallback image z SPIFFS
bool loadFallbackImageFromSPIFFS() {
  extern TFT_eSPI tft;
  
  Serial.println("🛡️ Ładuję fallback image z SPIFFS...");
  
  // Sprawdź czy SPIFFS jest zamountowany
  if (!SPIFFS.begin()) {
    Serial.println("❌ SPIFFS mount failed");
    return false;
  }
  
  // Sprawdź czy plik istnieje
  if (!SPIFFS.exists("/fallback_error_img.jpg")) {
    Serial.println("❌ Fallback image nie istnieje w SPIFFS");
    return false;
  }
  
  // Otwórz plik
  File file = SPIFFS.open("/fallback_error_img.jpg", "r");
  if (!file) {
    Serial.println("❌ Nie można otworzyć fallback image");
    return false;
  }
  
  size_t fileSize = file.size();
  Serial.println("📦 Fallback image size: " + String(fileSize) + " bytes");
  
  // Alokuj buffer
  uint8_t* buffer = (uint8_t*)malloc(fileSize);
  if (!buffer) {
    Serial.println("❌ Brak pamięci dla fallback image");
    file.close();
    return false;
  }
  
  // Czytaj plik
  file.read(buffer, fileSize);
  file.close();
  
  // Wyczyść ekran
  tft.fillScreen(TFT_BLACK);
  
  // Setup TJpg_Decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output_nasa);
  
  // Dekoduj fallback image
  int result = TJpgDec.drawJpg(0, 0, buffer, fileSize);
  
  if (result == 0) {
    Serial.println("✅ Fallback image załadowany pomyślnie!");
    
    // Dodaj napis "FALLBACK IMAGE"
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("FALLBACK IMAGE - NASA CONNECTION ERROR", tft.width() / 2, 230);
    
    free(buffer);
    return true;
  } else {
    Serial.println("❌ Fallback image decode error: " + String(result));
    free(buffer);
    return false;
  }
}

void initNASAImageSystem() {
  Serial.println("=== INICJALIZACJA NASA ULTIMATE SYSTEM ===");
  Serial.println("Calkowita kolekcja: " + String(num_nasa_images) + " obrazków");
  Serial.println("RANDOM MODE: Losowe obrazki co " + String(IMAGE_CHANGE_INTERVAL/1000) + " sekund");
  
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
  
  // LOSOWY WYBÓR ze wszystkich 1359 obrazków
  currentImage.imageNumber = random(0, num_nasa_images); // 0-1358 (losowy)
  
  if (TEST_IMG == 1)
  {
   Serial.println("podmieniam");
   currentImage.url = "https://roccoss39.github.io/nasa.github.io-/nasa-images/Colorful_Airglow_Bands_Surround_Milky_Way.jpg";
  }
  else
  currentImage.url = String(nasa_ultimate_collection[currentImage.imageNumber].url);
  
  currentImage.title = String(nasa_ultimate_collection[currentImage.imageNumber].title);
  currentImage.date = String(nasa_ultimate_collection[currentImage.imageNumber].filename); // filename as date
  currentImage.lastUpdate = millis();
  
  Serial.println("=== 🎲 RANDOM NASA " + String(currentImage.imageNumber + 1) + "/" + String(num_nasa_images) + " (MEGA COLLECTION) ===");
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
  
  if (firstRun || (millis() - lastImageChange >= IMAGE_CHANGE_INTERVAL)) {
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
      Serial.println("❌ Nie udało się wyświetlić NASA obrazka - próbuję inny...");
      
      // RETRY: Spróbuj 2 razy z różnymi obrazkami
      bool retrySuccess = false;
      for (int retry = 0; retry < 2 && !retrySuccess; retry++) {
        int retryImage = random(0, min(50, num_nasa_images)); // Pierwsze 50 obrazków (najbardziej stabilne)
        Serial.println("🔄 RETRY " + String(retry + 1) + "/2: NASA #" + String(retryImage + 1));
        
        if (downloadAndDisplayImage(tft, retryImage)) {
          Serial.println("✅ RETRY sukces!");
          currentImage.imageNumber = retryImage;
          lastImageChange = millis();
          firstRun = false;
          retrySuccess = true;
        }
      }
      
      if (!retrySuccess) {
        Serial.println("❌ Wszystkie RETRY nieudane - próbuję fallback image");
        
        // Spróbuj załadować fallback image z SPIFFS
        if (loadFallbackImageFromSPIFFS()) {
          Serial.println("✅ Fallback image załadowany - problem rozwiązany!");
          firstRun = false;
          lastImageChange = millis();
          return; // Sukces - zakończ funkcję
        }
        
        Serial.println("❌ Fallback image też nie działa - pokazuję error screen");
      }
      
      // Jeśli wszystko zawiedzie, pokaż tekstowy error
      tft.fillScreen(COLOR_BACKGROUND);
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("CRITICAL ERROR", tft.width() / 2, tft.height() / 2 - 30);
      tft.setTextSize(1);
      tft.drawString("NASA Connection Failed", tft.width() / 2, tft.height() / 2 - 10);
      tft.drawString("Fallback Image Failed", tft.width() / 2, tft.height() / 2 + 10);
      tft.drawString("Check SPIFFS & Internet", tft.width() / 2, tft.height() / 2 + 30);
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
  
  // Pokaż loading screen z ikoną ładowania
  tft.fillScreen(TFT_BLACK);
  
  // Ikona ładowania - animowany spinner
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;
  
  // Narysuj spinner (kółko z linią)
  tft.drawCircle(centerX, centerY, 20, TFT_CYAN);
  tft.drawCircle(centerX, centerY, 15, TFT_BLUE);
  tft.drawLine(centerX, centerY, centerX + 15, centerY - 10, TFT_WHITE);
  
  // Bez tekstu - tylko ikona ładowania
  
  // HTTP download przez HTTPClient (prostsze niż WiFiClientSecure)
  HTTPClient http;
  
  // === KONFIGURACJA TIMEOUTS ===
  http.setTimeout(10000); // 10 sekund timeout
  http.setConnectTimeout(5000); // 5 sekund na połączenie

    if (TEST_IMG == 1)
  {
   Serial.println("podmieniam");
   selectedImage.url = "https://roccoss39.github.io/nasa.github.io-/nasa-images/Colorful_Airglow_Bands_Surround_Milky_Way@@@@@.jpg";
  }

  Serial.printf("🌐 Connecting to: %s\n", selectedImage.url);
  http.begin(selectedImage.url);
  
  Serial.println("🌐 Sending HTTP GET request...");
  unsigned long startTime = millis();
  int httpCode = http.GET();
  unsigned long requestTime = millis() - startTime;
  
  Serial.printf("🌐 HTTP response: %d (took %lu ms)\n", httpCode, requestTime);
  
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
  
  // === SPRAWDŹ WSZYSTKIE HEADERS ===
  String contentType = http.header("Content-Type");
  String server = http.header("Server");
  String lastModified = http.header("Last-Modified");
  
  Serial.printf("🔍 Content-Type: '%s'\n", contentType.c_str());
  Serial.printf("🔍 Server: '%s'\n", server.c_str());
  Serial.printf("🔍 Last-Modified: '%s'\n", lastModified.c_str());
  
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.println("❌ No stream available");
    http.end();
    return false;
  }
  
  // Pobierz pierwsze 20 bajtów do sprawdzenia
  uint8_t testBuffer[20];
  size_t testRead = stream->readBytes(testBuffer, min(20, contentLength));
  
  Serial.print("🔍 First 20 bytes HEX: ");
  for(int i = 0; i < testRead; i++) {
    Serial.printf("%02X ", testBuffer[i]);
  }
  Serial.println();
  
  Serial.print("🔍 First 20 bytes ASCII: ");
  for(int i = 0; i < testRead; i++) {
    char c = testBuffer[i];
    Serial.print((c >= 32 && c <= 126) ? c : '?');
  }
  Serial.println();
  
  // Sprawdź czy to JPEG (FF D8) w pierwszych bajtach
  bool looksLikeJPEG = (testRead >= 2 && testBuffer[0] == 0xFF && testBuffer[1] == 0xD8);
  Serial.printf("🔍 Looks like JPEG: %s\n", looksLikeJPEG ? "YES" : "NO");
  
  if (!looksLikeJPEG) {
    Serial.println("❌ ERROR: Data doesn't look like JPEG!");
    http.end();
    return false;
  }
  
  // Download to buffer (już mamy pierwsze 20 bajtów w testBuffer)
  uint8_t* buffer = (uint8_t*)malloc(contentLength);
  if (!buffer) {
    Serial.println("❌ Memory allocation failed");
    http.end();
    return false;
  }
  
  // === BLOKADA WiFi AUTO-RECONNECT PODCZAS POBIERANIA ===
  extern bool isImageDownloadInProgress; // Flaga z main.cpp
  isImageDownloadInProgress = true;
  Serial.println("🔒 WiFi auto-reconnect BLOCKED during image download");
  
  // === SPRAWDZENIE KOMPLETNOŚCI TRANSFERU ===
  Serial.println("🔄 Starting download...");
  
  // Skopiuj już przeczytane 20 bajtów na początek bufora
  memcpy(buffer, testBuffer, testRead);
  
  // Pobierz resztę danych (contentLength - testRead)
  size_t remainingBytes = contentLength - testRead;
  size_t bytesRead = testRead; // Już mamy pierwsze 20 bajtów
  
  if (remainingBytes > 0) {
    Serial.printf("🔍 About to read %d remaining bytes...\n", remainingBytes);
    Serial.printf("🔍 Free heap before read: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("🔍 Stream available: %d bytes\n", stream->available());
    
    unsigned long readStartTime = millis();
    
    // === BEZPIECZNY readBytes() z timeout monitoring ===
    size_t additionalRead = 0;
    size_t chunkSize = min(remainingBytes, (size_t)1024); // Czytaj po 1KB chunks
    
    while (additionalRead < remainingBytes) {
      // Sprawdź timeout
      if (millis() - readStartTime > 8000) { // 8s timeout
        Serial.printf("⚠️ readBytes() TIMEOUT after %lu ms\n", millis() - readStartTime);
        break;
      }
      
      // Sprawdź czy stream ma dane
      if (stream->available() == 0) {
        Serial.printf("⚠️ Stream empty - server stopped sending (got %d/%d bytes)\n", 
                      additionalRead, remainingBytes);
        delay(100); // Krótka pauza
        if (stream->available() == 0) break; // Jeśli nadal puste, kończymy
      }
      
      // Czytaj kolejny chunk
      size_t toRead = min(chunkSize, remainingBytes - additionalRead);
      size_t chunkRead = stream->readBytes(buffer + testRead + additionalRead, toRead);
      additionalRead += chunkRead;
      
      // Debug co 2KB
      if (additionalRead % 2048 == 0 || chunkRead == 0) {
        Serial.printf("🔄 Chunk progress: %d/%d bytes\n", additionalRead, remainingBytes);
        if (chunkRead == 0) break; // Jeśli chunk pusty, kończymy
      }
    }
    
    unsigned long readDuration = millis() - readStartTime;
    
    bytesRead += additionalRead;
    Serial.printf("🔄 Downloaded: %d + %d = %d bytes (took %lu ms)\n", 
                  testRead, additionalRead, bytesRead, readDuration);
    
    Serial.printf("🔍 Stream available after read: %d bytes\n", stream->available());
    Serial.printf("🔍 Free heap after read: %d bytes\n", ESP.getFreeHeap());
    
    // Sprawdź czy stream nadal ma dane ale readBytes() przestał
    if (additionalRead < remainingBytes && stream->available() > 0) {
      Serial.printf("⚠️ SUSPECT: readBytes() stopped early! Stream still has %d bytes\n", 
                    stream->available());
    }
  }
  
  // === ODBLOKOWANIE WiFi AUTO-RECONNECT ===
  isImageDownloadInProgress = false;
  Serial.println("🔓 WiFi auto-reconnect UNBLOCKED after download");
  
  Serial.printf("🔍 Transfer complete: %d/%d bytes (%.1f%%)\n", 
                bytesRead, contentLength, (bytesRead * 100.0) / contentLength);
  
  if (bytesRead != contentLength) {
    Serial.printf("❌ INCOMPLETE TRANSFER! Missing %d bytes\n", contentLength - bytesRead);
    
    // === RETRY MECHANISM dla niekompletnych transferów ===
    Serial.println("🔄 Attempting transfer retry...");
    free(buffer);
    http.end();
    
    // Krótka pauza przed retry
    delay(500);
    
    // Drugi próba z nowym połączeniem
    Serial.printf("🔄 RETRY: Reconnecting to %s\n", selectedImage.url);
    http.setTimeout(15000); // Dłuższy timeout dla retry
    http.setConnectTimeout(8000);
    http.begin(selectedImage.url);
    
    int retryCode = http.GET();
    if (retryCode != HTTP_CODE_OK) {
      Serial.printf("❌ RETRY failed: HTTP %d\n", retryCode);
      http.end();
      return false;
    }
    
    int retryContentLength = http.getSize();
    Serial.printf("🔄 RETRY: File size: %d bytes\n", retryContentLength);
    
    WiFiClient* retryStream = http.getStreamPtr();
    if (!retryStream) {
      Serial.println("❌ RETRY: No stream available");
      http.end();
      return false;
    }
    
    // Nowy buffer dla retry
    buffer = (uint8_t*)malloc(retryContentLength);
    if (!buffer) {
      Serial.println("❌ RETRY: Memory allocation failed");
      http.end();
      return false;
    }
    
    // Pełne pobieranie w retry (bez pre-check)
    Serial.println("🔄 RETRY: Starting full download...");
    Serial.printf("🔍 RETRY: Free heap before read: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("🔍 RETRY: Stream available: %d bytes\n", retryStream->available());
    
    isImageDownloadInProgress = true;
    
    unsigned long retryStartTime = millis();
    
    // === BEZPIECZNY RETRY readBytes() z timeout monitoring ===
    size_t retryBytesRead = 0;
    size_t retryChunkSize = min((size_t)retryContentLength, (size_t)1024);
    
    while (retryBytesRead < retryContentLength) {
      // Sprawdź timeout (dłuższy dla retry)
      if (millis() - retryStartTime > 12000) { // 12s timeout dla retry
        Serial.printf("⚠️ RETRY readBytes() TIMEOUT after %lu ms\n", millis() - retryStartTime);
        break;
      }
      
      // Sprawdź czy stream ma dane
      if (retryStream->available() == 0) {
        Serial.printf("⚠️ RETRY: Stream empty - server stopped sending (got %d/%d bytes)\n", 
                      retryBytesRead, retryContentLength);
        delay(100);
        if (retryStream->available() == 0) break;
      }
      
      // Czytaj kolejny chunk
      size_t retryToRead = min(retryChunkSize, (size_t)(retryContentLength - retryBytesRead));
      size_t retryChunkRead = retryStream->readBytes(buffer + retryBytesRead, retryToRead);
      retryBytesRead += retryChunkRead;
      
      // Debug co 2KB
      if (retryBytesRead % 2048 == 0 || retryChunkRead == 0) {
        Serial.printf("🔄 RETRY Chunk progress: %d/%d bytes\n", retryBytesRead, retryContentLength);
        if (retryChunkRead == 0) break;
      }
    }
    
    unsigned long retryDuration = millis() - retryStartTime;
    
    isImageDownloadInProgress = false;
    
    Serial.printf("🔍 RETRY: Read took %lu ms\n", retryDuration);
    Serial.printf("🔍 RETRY: Stream available after read: %d bytes\n", retryStream->available());
    Serial.printf("🔍 RETRY: Free heap after read: %d bytes\n", ESP.getFreeHeap());
    
    // Sprawdź czy retry też miał ten sam problem
    if (retryBytesRead < retryContentLength && retryStream->available() > 0) {
      Serial.printf("⚠️ RETRY SUSPECT: readBytes() stopped early! Stream still has %d bytes\n", 
                    retryStream->available());
    }
    
    Serial.printf("🔄 RETRY: Transfer complete: %d/%d bytes (%.1f%%)\n", 
                  retryBytesRead, retryContentLength, (retryBytesRead * 100.0) / retryContentLength);
    
    if (retryBytesRead != retryContentLength) {
      Serial.printf("❌ RETRY also failed! Missing %d bytes\n", retryContentLength - retryBytesRead);
      free(buffer);
      http.end();
      return false;
    }
    
    contentLength = retryContentLength;
    bytesRead = retryBytesRead;
    Serial.println("✅ RETRY successful - proceeding with image decode");
  }
  
  Serial.println("✅ Full transfer completed successfully");
  
  // Clear and display image
  tft.fillScreen(TFT_BLACK);
  
  // Setup TJpgDecoder with RGB swap for correct colors
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);  // Fix purple/violet colors
  TJpgDec.setCallback(tft_output_nasa);
  
  // DODAJ DEBUGOWANIE PRZED DEKODOWANIEM
  // === BARDZO SZCZEGÓŁOWY DEBUG BUFORA ===
  Serial.printf("📊 Buffer info: %d bytes, first 4 bytes: %02X %02X %02X %02X\n", 
                contentLength, buffer[0], buffer[1], buffer[2], buffer[3]);
  
  // Debug pierwszych 16 bajtów
  Serial.print("🔍 First 16 bytes: ");
  for(int i = 0; i < min(16, (int)contentLength); i++) {
    Serial.printf("%02X ", buffer[i]);
  }
  Serial.println();
  
  // Debug ostatnich 16 bajtów
  Serial.print("🔍 Last 16 bytes: ");
  for(int i = max(0, (int)contentLength - 16); i < (int)contentLength; i++) {
    Serial.printf("%02X ", buffer[i]);
  }
  Serial.println();
  
  // Sprawdź czy to faktycznie JPEG
  bool isValidJPEG = (buffer[0] == 0xFF && buffer[1] == 0xD8);
  bool hasValidEnd = (contentLength >= 2 && buffer[contentLength-2] == 0xFF && buffer[contentLength-1] == 0xD9);
  Serial.printf("🔍 JPEG validation: Start=%s, End=%s\n", 
                isValidJPEG ? "OK" : "FAIL", hasValidEnd ? "OK" : "FAIL");
  
  // Sprawdź wielkość vs limit
  Serial.printf("🔍 Memory check: Buffer=%d bytes, Free heap=%d bytes\n", 
                contentLength, ESP.getFreeHeap());
                
  // Sprawdź czy bufor nie jest uszkodzony
  uint32_t checksum = 0;
  for(size_t i = 0; i < contentLength; i++) {
    checksum += buffer[i];
  }
  Serial.printf("🔍 Buffer checksum: %08X\n", checksum);
  
  // Sprawdź czy to prawdziwy JPEG (powinien zaczynać się od FF D8)
  if (contentLength < 4 || buffer[0] != 0xFF || buffer[1] != 0xD8) {
    Serial.println("❌ BŁĄD: To nie jest poprawny JPEG!");
    Serial.printf("Expected FF D8, got %02X %02X\n", buffer[0], buffer[1]);
    free(buffer);
    http.end();
    return false;
  }
  
  // === DEBUG PRZED DEKODOWANIEM ===
  Serial.println("🎯 Starting JPEG decode...");
  Serial.printf("🎯 TJpgDec library ready, buffer at: 0x%08X\n", (uint32_t)buffer);
  Serial.printf("🎯 About to call TJpgDec.drawJpg(0, 0, buffer, %d)\n", contentLength);
  
  int result = TJpgDec.drawJpg(0, 0, buffer, contentLength);
  
  // === DEBUG PO DEKODOWANIU ===
  Serial.printf("🎯 JPEG decode completed with result: %d\n", result);
  Serial.printf("🎯 Free heap after decode: %d bytes\n", ESP.getFreeHeap());
  
  if (result == 0) {
    // Dodaj tytuł na dole - wyśrodkowany
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM); // Wyśrodkowanie
    
    String titleStr = String(selectedImage.title);
    if (titleStr.length() > 45) {
      titleStr = titleStr.substring(0, 42) + "...";
    }
    
    // Wyśrodkowany na dole ekranu
    tft.drawString(titleStr, tft.width() / 2, 225);
    
    // Usuń progress info - tylko czysty obrazek
    
    Serial.println("✅ NASA image displayed successfully!");
    free(buffer);
    http.end();
    return true;
  } else {
    // LEPSZE DEBUGOWANIE BŁĘDÓW JPEG
    String errorMsg;
    switch(result) {
      case 1: errorMsg = "Interrupted by output function"; break;
      case 2: errorMsg = "Device error or wrong termination"; break;
      case 3: errorMsg = "Insufficient memory pool"; break;
      case 4: errorMsg = "Insufficient stream input buffer"; break;
      case 5: errorMsg = "Parameter error"; break;
      case 6: errorMsg = "Data format error (not JPEG file)"; break;
      case 7: errorMsg = "Right format but not supported"; break;
      case 8: errorMsg = "Not supported JPEG standard"; break;
      default: errorMsg = "Unknown error"; break;
    }
    
    Serial.println("✗ JPEG decode error: " + String(result) + " - " + errorMsg);
    Serial.println("📄 URL: " + String(selectedImage.url));
    Serial.println("📦 Content-Length: " + String(contentLength));
    
    // === DODATKOWY DEBUG PRZY BŁĘDZIE ===
    Serial.println("🔴 DECODE FAILED - Additional debug:");
    Serial.printf("🔴 Buffer pointer valid: %s\n", buffer ? "YES" : "NO");
    Serial.printf("🔴 Content length > 0: %s\n", contentLength > 0 ? "YES" : "NO");
    
    // Sprawdź czy problem w środku pliku - szukaj JFIF/EXIF markerów
    bool hasJFIFMarker = false;
    bool hasEXIFMarker = false;
    for(size_t i = 0; i < min((size_t)100, (size_t)(contentLength-4)); i++) {
      if(buffer[i] == 'J' && buffer[i+1] == 'F' && buffer[i+2] == 'I' && buffer[i+3] == 'F') {
        hasJFIFMarker = true;
        Serial.printf("🔴 JFIF marker found at offset: %d\n", i);
      }
      if(buffer[i] == 'E' && buffer[i+1] == 'x' && buffer[i+2] == 'i' && buffer[i+3] == 'f') {
        hasEXIFMarker = true;
        Serial.printf("🔴 EXIF marker found at offset: %d\n", i);
      }
    }
    Serial.printf("🔴 JFIF marker present: %s\n", hasJFIFMarker ? "YES" : "NO");
    Serial.printf("🔴 EXIF marker present: %s\n", hasEXIFMarker ? "YES" : "NO");
    
    // Sprawdź czy są dodatkowe JPEG markery
    int markerCount = 0;
    for(size_t i = 0; i < contentLength-1; i++) {
      if(buffer[i] == 0xFF && buffer[i+1] != 0x00 && buffer[i+1] != 0xFF) {
        markerCount++;
        if(markerCount <= 5) { // Pokaż tylko pierwsze 5
          Serial.printf("🔴 JPEG marker at %d: FF %02X\n", i, buffer[i+1]);
        }
      }
    }
    Serial.printf("🔴 Total JPEG markers found: %d\n", markerCount);
    
    // FALLBACK: Pokaż error screen z detalami
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("JPEG ERROR " + String(result), 10, 10);
    tft.drawString(errorMsg, 10, 30);
    tft.drawString("Size: " + String(contentLength) + " bytes", 10, 50);
    tft.drawString("NASA #" + String(imageIndex + 1), 10, 70);
    
    free(buffer);
    http.end();
    return false;
  }
}