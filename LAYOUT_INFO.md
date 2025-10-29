# 🎨 Nowy Layout Stacji Pogodowej

## 📱 **Układ ekranu 320x240:**

```
┌──────────────────────────────────────────────────────────────┐
│ LEWA STRONA (170px)         │  PRAWA STRONA (140px)          │
│                            │                                │
│  ⏰ 14:25:33 (duży żółty)   │  ☀️ [IKONA]    22.5°C (duży)  │
│                            │                                │
│  📅 15.12.2024 (biały)     │  pochmurnie (cyjan)           │
│                            │                                │
│  📆 Poniedziałek (szary)    │  Wilgotność: 65% (biały)     │
│                            │                                │
│  📡 WiFi: OK (zielony)      │  Wiatr: 3.2 m/s (biały)      │
│                            │                                │
│                            │  Aktualizacja: 2 min temu     │
│                            │  (szary)                       │
└──────────────────────────────────────────────────────────────┘
```

## 🎯 **Poprawki layoutu:**

### **Lewa strona - Informacje czasowe:**
- ✅ **Duży zegar** (4x większy) - lepiej widoczny
- ✅ **Data** pod zegarem - logiczne ułożenie  
- ✅ **Dzień tygodnia** - dodatkowa informacja
- ✅ **Status WiFi** - diagnostyka połączenia

### **Prawa strona - Pogoda:**
- ✅ **Ikona pogody** - wizualna reprezentacja warunków
- ✅ **Temperatura** obok ikony - główna informacja
- ✅ **Opis** pod ikoną - dodatkowy kontekst
- ✅ **Parametry pogodowe** - wilgotność, wiatr
- ✅ **Czas aktualizacji** - informacja o świeżości danych

## 🌤️ **Ikony pogodowe:**

### **Słońce** ☀️:
- Słowa kluczowe: "słon", "jas"
- Wygląd: Żółte koło z promieniami

### **Chmury** ☁️:
- Słowa kluczowe: "chmur", "pochmur"  
- Wygląd: Białe/szare kółka

### **Deszcz** 🌧️:
- Słowa kluczowe: "deszcz", "opad"
- Wygląd: Chmura + niebieskie krople

### **Śnieg** ❄️:
- Słowa kluczowe: "śnieg"
- Wygląd: Chmura + białe płatki

### **Mgła** 🌫️:
- Słowa kluczowe: "mgła", "zamgl"
- Wygląd: Poziome szare linie

### **Inne** ❓:
- Nierozpoznane warunki
- Wygląd: Znak zapytania

## 🎨 **Kolory:**

- **Czas**: 🟡 Żółty (TFT_YELLOW) - główny akcent
- **Data**: ⚪ Biały (TFT_WHITE) - czytelność
- **Temperatura**: 🟠 Pomarańczowy (TFT_ORANGE) - ciepły
- **Pogoda**: 🔵 Cyjan (TFT_CYAN) - chłodny  
- **Detale**: 🔘 Szary (TFT_LIGHTGREY) - subtelne
- **WiFi OK**: 🟢 Zielony (TFT_GREEN) - status pozytywny
- **WiFi ERROR**: 🔴 Czerwony (TFT_RED) - status negatywny

## ⚡ **Optymalizacje:**

- **Selektywne odświeżanie** - tylko zmienione obszary
- **Jednokrotne czyszczenie** - cały blok na raz
- **Proper text alignment** - TL_DATUM dla lewego rozmieszczenia
- **Responsive design** - dostosowane do 320x240