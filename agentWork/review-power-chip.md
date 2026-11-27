---
STATUS: COMPLETE
ZADANIE: Review nakladki zasilania (power chip) w flushStampedFrame - TamaPoke.ino, power_status.h, i18n, testy.
AKTUALIZACJA: 2026-11-27
---

## Punkt wznowienia
Review zakonczone; nastepny krok to decyzja autora co do zastrzezen N1-N6 (brak blokujacych).

## Stan pracy
[x] diff + power_status.h
[x] sciezki rysowania (pyt. 1)
[x] geometria (pyt. 2)
[x] redraw (pyt. 3)
[x] semantyka AXP2101 (pyt. 4) - XPowersLib src sprawdzony
[x] reguly projektu (pyt. 5)

## Sprawdzone zrodla
- ~/Documents/Arduino/libraries/XPowersLib/src/XPowersAXP2101.tpp:292-319 (isCharging, isVbusIn), :2392 (getBatteryPercent)
- rtcbat.cpp:79-100 (cache 2s/10s)
- tests: power_status_tests OK, i18n_tests 7x245 OK; git diff --check czysty

## Ustalenia
Blokujace: brak.
N1 kontrast UI_TRACK na bialej pigulce (stan BATTERY >=40%): wypelnienie ikony prawie niewidoczne (TamaPoke.ino ~1580).
N2 blad I2C -> getBatteryPercent=-1 -> pigulka znika na okres cache + pelny redraw statycznych ekranow.
N3 brak clampu pc w drawPowerChip (fw liczone z surowego pc).
N4 pigulka nad STEP_HUD z 1 px przerwy, przesunieta poziomo - wizualny tlok na glownym ekranie.
N5 opoznienie reakcji na USB do ~10.6 s w powerSave.
N6 test power_status - granica reguly "tylko krytyczne".
Szczegoly w raporcie dla koordynatora.

## Otwarte pytania
- Wszystko co sprzetowe (patrz raport).
