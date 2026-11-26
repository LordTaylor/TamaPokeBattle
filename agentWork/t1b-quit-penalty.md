---
STATUS: COMPLETE
ZADANIE: Kara -1 za ucieczke z walki PvP (trainer + Pet + flaga NVS + testy + docs)
AKTUALIZACJA: 2026-11-27
---
## Punkt wznowienia
Review i PR (koordynator).
## Stan pracy
[x] trainerApplyQuitPenalty, PVP_QUIT_PENALTY; [x] Pet::recordPvpQuit/pvpBattleBegin/End/applyPendingBattleQuit, klucz "pvpbat"; [x] testy [x] docs [x] bramki (make test 0; firmware 787850 B) [x] mutacje (clamp/l50 rownowazne przy karze 1; zero, noload, noflagremove, noend, noimport, poziom-spada lapane)
## Decyzje
- save() kary, potem remove flagi (przerwanie = max druga kara, nie darmowa ucieczka)
- recordPvpQuit nie dotyka W/L/D/historii
- importBackup kasuje flage; factoryReset = prefs.clear
## Sprawdzone zrodla
pet.cpp (brak kolizji klucza pvpbat)
## Ustalenia
## Otwarte pytania

## Poprawki po review Opusa (2026-11-27)
Rebase na a2772b2; lustro pvpBattleActive, recordPvpQuit z guardem flagi i zapisem samego tprog, recordPvpMatch kasuje flage, clearBattleFlag z isKey, docs (kontrakt, D3, D5, W4), testy. make test 0; firmware 788034 B; mutacje (nomatchclear, noguard, noquitclear, noload) lapane.
