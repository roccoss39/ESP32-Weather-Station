# 🌤️ Stacja Pogodowa - INSTRUKCJA

## ✅ **Co zostało stworzone:**

### **Nowy kod pogodowy** (`src/main.cpp`):
- **Zachowuje działającą konfigurację TFT** 
- **Dodaje funkcje pogodowe** z OpenWeatherMap API
- **Bezpieczne aktualizacje** bez konfliktów

### **Struktura ekranu:**
```
┌─────────────────────────────────────┐
│ 14:25:33              22.5°C       │
│ 15.12.2024            pochmurnie   │
│                       Wilg: 65%    │
│                       Wiatr: 3.2m/s│
│                       Kier: SW     │
│                       Akt: 2min    │
└─────────────────────────────────────┘
```

## ⚙️ **KONFIGURACJA - WAŻNE:**

### **1. Sprawdź ustawienia w kodzie:**
```cpp
const char* ssid = "zero";
const char* password = "Qweqweqwe1";  // ✅ Sprawdź hasło WiFi!
const char* weatherApiKey = "ac44d6e8539af12c769627cbdfbbbe56";  // ✅ Sprawdź klucz API!
const char* city = "Szczecin";  // ✅ Zmień na swoje miasto
```

### **2. Klucz OpenWeatherMap API:**
- **Darmowy** na [openweathermap.org](https://openweathermap.org/api)
- **Rejestracja** → **API keys** → **Skopiuj klucz**
- **Wklej** w `weatherApiKey`

## 🚀 **INSTALACJA:**

### **Krok 1: Kompilacja**
```bash
# W PlatformIO IDE kliknij Build
pio run
```

### **Krok 2: Upload**
```bash
# Wgraj na ESP32
pio run --target upload
```

### **Krok 3: Monitor (opcjonalnie)**
```bash
# Sprawdź logi
pio device monitor
```

## 📊 **Funkcje stacji pogodowej:**

### **Wyświetlane dane:**
- 🌡️ **Temperatura** (pomarańczowy, duży tekst)
- 🌤️ **Opis pogody** (cyjan, po polsku)
- 💧 **Wilgotność** (biały)
- 💨 **Prędkość wiatru** (biały)
- 🧭 **Kierunek wiatru** (N, NE, E, SE, S, SW, W, NW)
- ⏱️ **Czas ostatniej aktualizacji** (szary)

### **Automatyczne funkcje:**
- ⏰ **Zegar** aktualizowany co sekundę
- 🌐 **Pogoda** aktualizowana co 10 minut
- 🔄 **Auto-retry** przy błędach WiFi/API
- 🚨 **Komunikaty błędów** na ekranie

## 🔍 **Diagnostyka:**

### **Jeśli brak pogody:**
1. **Sprawdź Serial Monitor** - logi błędów
2. **Sprawdź WiFi** - czy ESP32 się łączy
3. **Sprawdź API key** - czy jest poprawny
4. **Sprawdź miasto** - czy nazwa jest poprawna

### **Jeśli zegar nie działa:**
- **Konfiguracja TFT pozostała ta sama** - powinien działać
- **Sprawdź połączenie WiFi** - czas pobierany z NTP

## 🎨 **Kolory:**

- **Czas**: 🟡 Żółty (TFT_YELLOW)
- **Data**: ⚪ Biały (TFT_WHITE)  
- **Temperatura**: 🟠 Pomarańczowy (TFT_ORANGE)
- **Pogoda**: 🔵 Cyjan (TFT_CYAN)
- **Błędy**: 🔴 Czerwony (TFT_RED)

## ⚡ **Gotowe do testowania!**

**Sprawdź konfigurację WiFi i API, następnie wgraj kod na ESP32!**