---
STATUS: COMPLETE
ZADANIE: Pomiary T4.0 na jednej plytce (NVSSTAT, RADIOCYC) przed T4.2
AKTUALIZACJA: 2026-11-27
---

## Punkt wznowienia
Uzupelnic PVPPING na dwoch Waveshare, gdy bedzie druga plytka. T4.2 mozna zaczynac (bramki 1-plytkowe PASS).

## Wyniki (Waveshare ESP32-S3 AMOLED 1.75, build TAMAPOKE_LOCAL_TEST z feat/power-chip)
- NVS: used=197 free=433 avail=307 total=630, avail 60 % (bramka >=55 %: PASS), erased=0 po 10 cyklach
- Namespace'y: tamapoke (91 kluczy), nvs.net80211 (2), phy (3) - bez przyrostu po cyklach
- Sterta po stop: 245476 -> 245464 B po 10 cyklach (wyciek <2 KB: PASS)
- Najwiekszy wolny blok: 172020 B (>=61440: PASS)
- start() WiFi+ESP-NOW: 43 ms pierwszy, 10 ms kolejne (<1500 ms: PASS)

## Zastrzezenia
- Plytka miala juz wpisy phy/nvs.net80211 z wczesniejszego szkicu ESP-NOW, wiec 43 ms to NIE jest start na swiezej plytce bez kalibracji PHY; prawdziwy zimny start moze byc dluzszy i zapisac blob PHY w NVS
- PVPPING (2 plytki Waveshare) nie wykonany: druga Waveshare niedostepna
- Karta SD po kazdym flashu (reset RTS) nie montuje sie do pelnego cyklu zasilania (sd=0, sprite'y z SD niedostepne)

## Test lacza radiowego (inna plytka ESP32-S3 N16R8)
- Szkic ESP-NOW na obu: ~1 % strat, RSSI ok. -44 dBm z bliska
