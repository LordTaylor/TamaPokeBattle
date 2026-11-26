# Poziom trenera: model, zapis i uwagi do integracji

Logika: `trainer.h` / `trainer.cpp` (czysta, bez Arduino). Integracja: `Pet::recordPvpMatch`, zapis w NVS.

## Zasady
- Punkty: wygrana 3, remis 2, przegrana 1; wygrana z wyzszym poziomem +2, przegrana z nizszym -2. ABORTED nic nie zmienia.
- Poziom 1..50, nigdy nie spada; `progress` nie schodzi ponizej 0; na poziomie 50 `progress` = 0.
- FarmGuard: te same dwa urzadzenia daja punkty raz na godzine. Punkty sa blokowane tez gdy `nowEpoch == 0`
  (RTC niewazny) albo zegar cofnieto wzgledem zapisanego wpisu (wpis dostaje wtedy nowy czas, cooldown startuje od nowa).
- Mecz zablokowany przez guard nadal zwieksza W/L/D i `matchSeq` (`TrainerAward::blocked == true`). Do potwierdzenia przez wlasciciela.

## NVS
Klucze: `tlvl`, `tprog`, `twin`, `tlos`, `tdrw`, `tseq`, bloby `tfarm`, `thist`. Poza `PetBackup`.
Bloby to zrzut struktur bez wersji: zmiana layoutu `FarmGuard` / `PvpHistoryEntry` = nowy klucz.

## Uwagi dla integracji (T4)
- `oppDeviceId` MUSI pochodzic z MAC nadawcy z callbacku ESP-NOW, nigdy z pol payloadu (inaczej spoof daje nielimitowany farm).
- `oppNick` przekazuj z jawna dlugoscia (`oppNickLen`), bufor z pakietu nie musi miec terminatora; `oppLevel` jest przycinany do 1..50.
- `nowEpoch` = `rtcEpoch()`; 0 oznacza niewazny RTC i blokuje punkty (mecz nadal sie liczy).
- UI: rozroznij `award.blocked` (komunikat o limicie godzinowym) od `PVP_ABORTED` (brak rozliczenia).
