---
STATUS: COMPLETE
ZADANIE: Niezalezne review integracji UI PvP T4.2 (haki TamaPoke.ino, renderery, stringi) vs spec A.4/C/F/G.3
AKTUALIZACJA: 2026-11-27
---

## Punkt wznowienia
Review zakonczone; raport przekazany koordynatorowi. Nastepny krok: poprawka B1 (bramka w PvpApp::onBattleStart) i B2 (gitlink .claude/worktrees).

## Stan pracy
[x] 1 haki TamaPoke.ino
[x] 2 forceStop/leave/kara
[x] 3 UI renderery
[x] 4 pamiec/zywotnosc
[x] 5 AGENTS.md

## Decyzje podjete
## Sprawdzone zrodla
- docs/pvp-integration-spec.md A.4, B.7, C.0-C.6
- TamaPoke.ino diff, pvp_app.cpp, pvp_app_events.cpp, pvp_session_battle.cpp:19-52, pet.cpp:520-570, pvp_ui_effects.cpp, pvp_ui_ended.cpp, pvp_ui_card.cpp
## Ustalenia
- BLOKUJACE B1: straznik T4.2 (pvp_ui_effects.cpp:65-69 guardMissingBattleUi) jest martwy. PvpSession::onReady (pvp_session_battle.cpp:31-44) pushuje OPPONENT_READY i od razu BATTLE_START (tryEnterBattle) gdy lokalny READY juz wyslany - a onAccepted wysyla go synchronicznie. Oba zdarzenia schodza w jednym PvpPump::drain wewnatrz gApp.update(), zanim pvpUiTrack zobaczy PREPARING. Skutek: onBattleStart -> battleOpen_, pet.pvpBattleBegin() (zapis NVS flagi), faza CHOOSE -> ekran 'PvP wkrotce' z jedynym przyciskiem UCIEKAJ = quit() = kara -1; reboot = kara -1 (applyPendingBattleQuit); bez dotyku walka gra sie sama ACTION_TIMEOUT co 20 s az do FINISHED -> recordMatch (punkty, historia, odznaka). Swajpy ignorowane, PWR zablokowany (keepAwake). Sprzeczne ze spec A.3 T4.2 'bez BATTLE_START po stronie UI'.
- BLOKUJACE B1b: na ekranie 'PvP wkrotce' przycisk UCIEKAJ (END_OK 118..348 x 300..350) lezy w miejscu ANULUJ z PREPARING (133..333 x 300..344); tap ANULUJ tuz po BATTLE_START = quit() bez modala C.6 = kara -1 (pvp_ui_ended.cpp:80-89).
- BLOKUJACE B2: commit 30d76de dodal gitlink .claude/worktrees/agent-a94918148586e39ed (Subproject commit bez .gitmodules); w indeksie czeka kolejny (agent-aea0f4e9c798cdabb).
- Niekrytyczne: STOPPING rysuje pusta klatke (do 200 ms migniecie); denyText EGG/CEREMONY/NO_NICK -> ogolny napis; render co 200 ms takze na statycznym ENDED; pvp_ui_text_tests testChordTable/testCenter to testy geometrii (spec A.6 i regula globalna: nie piszemy) - zostawic testFit; ensureDailyGoals/applyWalk moga zrobic pelny save() w trakcie PvP (rzadko, <10 s timeout); toast VERSION w CHALLENGING ginie (potem i tak ekran ENDED UPDATE); powtorny ten sam notice w tej samej iteracji petli nie da toastu.
- OK: wszystkie wspolrzedne w kole r=233-8, nic nie wchodzi w pigulke y=7..23; kolory/wymiary z pvp_ui_theme.h; ASCII; pliki <150 linii; brak alokacji w petli; globale w pvp_app_instance.cpp w jednym TU (kolejnosc init OK); make test zielony (sprawdzone).
- Hak A.4 komplet: 1,2,3,3b,3c(jako warunek, ok),4,5,6,7,8,9,10 obecne. Kolejnosc loop OK.
## Otwarte pytania
