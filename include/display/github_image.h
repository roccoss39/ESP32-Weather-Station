#ifndef GITHUB_IMAGE_H
#define GITHUB_IMAGE_H

#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// --- STRUKTURA AKTUALNEGO OBRAZKA ---
struct CurrentImageData {
  String url = "";              // URL do zdjęcia
  String title = "";            // Tytuł obrazka
  String date = "";             // Data obrazka
  int imageNumber = 0;          // Numer zdjęcia (0-1358)
  bool isValid = false;         // Czy dane są ważne
  unsigned long lastUpdate = 0; // Kiedy ostatnio zaktualizowano
};

// --- ZMIENNE GLOBALNE ---
extern CurrentImageData currentImage;

// --- FUNKCJE ---
void initNASAImageSystem();
bool getRandomNASAImage();
void displayGitHubImage(TFT_eSPI& tft);
bool downloadAndDisplayImage(TFT_eSPI& tft, int imageIndex);

#endif