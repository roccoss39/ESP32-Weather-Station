# 🧪 Weather Station Test Mode

## Jak uruchomić tryb testowy:

### **1. Przełącz na tryb testowy:**
```bash
# Zmień nazwę plików
mv src/main.cpp src/main_normal.cpp
mv src/main_test_mode.cpp src/main.cpp
```

### **2. Skompiluj i wgraj:**
```bash
pio run --target upload
```

### **3. Otwórz Serial Monitor:**
```bash
pio device monitor
```

## 🎯 **Co testuje:**

### **12 scenariuszy pogodowych (co 4 sekundy):**

1. **SŁONECZNIE** - `01d` - bezchmurnie, wysokie ciśnienie
2. **LEKKIE CHMURY** - `02d` - zachmurzenie małe
3. **UMIARKOWANE CHMURY** - `03d` - zachmurzenie umiarkowane  
4. **DUŻE CHMURY** - `04d` - zachmurzenie duże
5. **LEKKI DESZCZ** - `10d` - deszcz + umiarkowany wiatr
6. **SILNY DESZCZ** - `09d` - deszcz + silny wiatr (czerwony)
7. **BURZA** - `11d` - burza + bardzo silny wiatr (bordowy)
8. **ŚNIEG** - `13d` - śnieg + silny wiatr
9. **MGŁA** - `50d` - mgła + spokojny wiatr
10. **WYSOKIE CIŚNIENIE** - `01d` - słońce + bardzo wysokie ciśnienie (magenta)
11. **EKSTREMALNE** - `13d` - zamieć + ekstremalny wiatr + niskie ciśnienie
12. **UPAŁ** - `01d` - bardzo wysoka temperatura

### **Testowane elementy:**
- ✅ **Ikony pogodowe** - wszystkie typy zachmurzenia i warunków
- ✅ **Kolory wiatru** - biały → żółty → czerwony → bordowy
- ✅ **Kolory ciśnienia** - pomarańczowy → biały → magenta
- ✅ **Opisy pogodowe** - polskie nazwy i skracanie
- ✅ **Temperatury ekstremalne** - od -8°C do +38°C
- ✅ **Cache system** - czy rysuje tylko przy zmianach
- ✅ **Walidacja danych** - zabezpieczenia przed błędami

## 📟 **Komendy Serial Monitor:**

```
r - Reset test cycle (rozpocznij od początku)
v - Validate current data (sprawdź poprawność danych)
s - Show current test info (pokaż info o aktualnym teście)
```

## 📊 **Debug output:**

```
=== TEST 6/12: SILNY DESZCZ ===
Temp: 14.7°C (odcz: 12.1°C)
Opis: 'deszcz silny'
Ikona: '09d'
Wilg: 90%
Wiatr: 22.5 km/h
Ciśn: 995 hPa

Wind: 22.5km/h - STRONG (czerwony)
Pressure: 995hPa - LOW (pomarańczowy)
Weather data changed - redrawing display
```

## 🔍 **Co sprawdzać:**

### **Na ekranie TFT:**
- 🎨 **Czy ikony** wyglądają dobrze dla każdego typu pogody
- 🌈 **Czy kolory** zmieniają się zgodnie z wartościami
- 📝 **Czy opisy** są dobrze skracane i czytelne
- ⚡ **Czy cache** działa (brak migotania)

### **W Serial Monitor:**
- 📊 **Debug info** - czy wszystkie dane się wyświetlają
- ✅ **Walidacja** - czy nie ma błędów zakresu
- 🔄 **Cykliczność** - czy test wraca do początku po 12 scenariuszu

## 🔧 **Jak wrócić do normalnego trybu:**

```bash
# Przywróć oryginalne pliki
mv src/main.cpp src/main_test_mode.cpp
mv src/main_normal.cpp src/main.cpp

# Wgraj normalny kod
pio run --target upload
```

## 🐛 **Troubleshooting:**

- **Brak WiFi:** Test działa offline, tylko czas nie będzie wyświetlany
- **Błędy kompilacji:** Sprawdź czy wszystkie pliki są w odpowiednich folderach
- **Dziwne ikony:** To normalne - niektóre są proste (kwadrat, koło)
- **Migotanie:** Oznacza problem z cache system

**Test jest idealny do sprawdzenia czy wszystkie ikony i kolory działają poprawnie!** 🎯