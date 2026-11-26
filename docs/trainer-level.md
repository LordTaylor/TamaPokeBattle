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

## Ucieczka z walki
Decyzja wlasciciela: ucieczka z walki PvP zawsze kosztuje uciekajacego `PVP_QUIT_PENALTY` (1) punkt postepu.
- `trainerApplyQuitPenalty`: `progress = max(0, progress - 1)`; poziom nigdy nie spada, na poziomie 50 bez efektu; W/L/D, `matchSeq` i historia bez zmian.
- Kare naklada lokalnie uciekajacy (`Pet::recordPvpQuit()`). Pakiet ABORT od przeciwnika nigdy nie karze odbiorcy.
- Zanik zasiegu / timeout nie karze nikogo (nie wiadomo, kto uciekl).
- Zamkniecie urzadzenia w trakcie walki: flaga `pvpbat` w NVS (poza `PetBackup`, `BACKUP_VERSION` bez zmian), ustawiana przez `pvpBattleBegin()`
  i kasowana przez `pvpBattleEnd()`. Jesli `Pet::begin()` zastanie flage, nalozy kare, zapisze ja (`save()`) i dopiero potem skasuje flage
  (przerwanie miedzy tymi krokami da co najwyzej druga kare, nigdy darmowa ucieczke). `importBackup` kasuje flage, `factoryReset` czysci cala NVS.

Kontrakt dla T4 (wszystkie wywolania z `loop()`, nigdy z callbacku ESP-NOW; maszyna stanow wymuszona w `Pet`: begin -> (wynik | quit) -> koniec):
- `pvpBattleBegin()` przy `PVP_EVT_BATTLE_START` (nie przy CHALLENGE/ACCEPT), przy starcie walki, nie w srodku animacji (zapis NVS). Idempotentne.
- `recordPvpQuit()` przy swiadomym wyjsciu gracza w trakcie walki, przed wynikiem. Kara tylko gdy flaga ustawiona (lustro RAM `pvpBattleActive`); zapisuje sam klucz `tprog`, potem kasuje flage. Po `recordPvpMatch` zwraca false bez kary.
- `recordPvpMatch()` kasuje flage po swoim `save()`: wynik i kara wykluczaja sie dla jednej walki, wylaczenie na ekranie wyniku nie karze.
- `pvpBattleEnd()` tylko przy timeoucie / ABORT od przeciwnika (bez kary).

Uwagi do decyzji wlasciciela:
- Crash / WDT / brownout w trakcie walki tez karze (flaga nie odroznia przyczyny); filtr po `esp_reset_reason` swiadomie pominiety ("ucieczka zawsze -1").
- Kara -1 rowna sie przegranej z nizszym poziomem, a ucieczka odbiera punkty przeciwnikowi: strona nieuciekajaca dostaje dzis 0 (ABORTED). Do decyzji.
