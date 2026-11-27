---
STATUS: COMPLETE
ZADANIE: Niezalezne review calej galezi feat/pvp-t42-radio (T4.2+T4.3) przed PR do main
AKTUALIZACJA: 2026-11-27
---

## Punkt wznowienia
Review zakonczone. Nastepny: poprawki N1-N3 (opcjonalnie przed PR), potem test sprzetowy G.2 wg listy "Niezweryfikowane".

## Stan pracy
[x] 0 status zastrzezen z review T4.2 (link, integracja)
[x] 1 przeplyw faz / dotyk / modal ucieczki
[x] 2 sprite rywala / SD
[x] 3 NVS w trakcie PvP
[x] 4 AGENTS.md / higiena repo
[x] 5 werdykt

## Sprawdzone zrodla
- git log main..HEAD (HEAD 6093808 merge T4.3), diff --stat 60 plikow
- pvp_app.cpp, pvp_app_events.cpp, pvp_app_instance.cpp, pvp_ui_app.cpp, pvp_ui_battle.cpp, pvp_ui_battle_scene.cpp,
  pvp_ui_quit.cpp, pvp_ui_fx.cpp, pvp_ui_effects.cpp, pvp_ui_result.cpp, pvp_ui_ended.cpp, pvp_ui_card.cpp, pvp_ui_theme.h
- TamaPoke.ino diff + drawPmdActM (5744), render() (1783), trackStepEncounter (2934); sdmon.cpp PmdMon::load; pet.cpp 395-580, 1639
- make clean && make test: EXIT=0 (sprawdzone samodzielnie)

## Ustalenia
### 0 Zastrzezenia z T4.2
- B1 (martwy straznik, walka bez UI): rozwiazane - jest ekran walki, PVP_BATTLE_ENABLED=true, bramka w onBattleStart dodana (f8ca550).
- B1b (ANULUJ z PREPARING = UCIEKAJ = kara): rozwiazane - przycisk ucieczki otwiera modal C.6. Resztkowo: ANULUJ (133..333 x 300..344)
  trafia w rzad QUICK/HEAVY (300..346) -> przypadkowy wybor akcji, bez kary. Niekrytyczne.
- B2 gitlinki: w HEAD brak wpisow 160000, .claude/worktrees/ w .gitignore. Rozwiazane. Lokalnie nadal 2 worktree agentow (git worktree list).
- Z review linku: brak esp_now_del_peer, set_channel bez sprawdzenia, +76 KB flash bez uzycia - nadal otwarte (niekrytyczne, T4.4).
### 1 Przeplyw
- quit(): tylko CHOOSE/WAIT_OPP/WAIT_CONFIRM i battleOpen_; finishQuit zeruje battleOpen_, recordQuit no-op bez flagi -> -1 dokladnie raz. OK.
- Modal: zostaje przez auto-timeout (WAIT_OPP), zamyka sie w WAIT_FINAL/RESULT/ENDED; STAY sprawdzany pierwszy. OK.
- WAIT_FINAL: brak przyciskow, ale sesja ma PEER_TIMEOUT 10 s / STALL 60 s -> nie ma stanu bez wyjscia. STOPPING: wyjscie po flush.
- NIEKRYTYCZNE (UX, realne): ENDED bez blokady tapniecia. OK konca (118..348 x 300..350) lezy pod QUICK/HEAVY i pod STAY modala.
  Rywal ucieka/zrywa lacze -> tap w akcje albo STAY (jeszcze zanim ENDED sie narysuje, render co 200 ms) = leave() od razu;
  gracz nie widzi "rywal uciekl +N pkt". RESULT ma PVP_RESULT_LOCK_MS, ENDED nie (pvp_ui_ended.cpp:76-80).
- NIEKRYTYCZNE: kolejnosc warstw - sprite'y rysowane PO paskach HP (pvp_ui_battle.cpp pvpUiDrawBattle). Ground 256, wysokosc do 170 px
  (drawPmdActM sBase=170/h) -> gorna krawedz do y=86, zaslania HP bar (112..126), liczby HP (130), "-N" (140), chipy typow.
- NIEKRYTYCZNE: RESULT OK 118..348 x 396..448 wychodzi poza kolo w dolnych rogach (przy x=118 brzeg kola y~436).
### 2 Sprite rywala / SD
- wildPmd.load() raz na walke (pvp_ui_fx.cpp pvpFxBattleStart, z pvpUiTrack po gApp.update). Odczyt pliku PMD (zwykle dziesiatki-setki KB,
  limit 3 MB) z SD_MMC: rzad 100-300 ms. Pompa traci jeden tick; RX ESP-NOW buforuje ring 16, retransmisje co 150 ms, PEER_TIMEOUT 10 s.
  Ryzyko dla sesji: niskie. Zegar rundy UI juz biegnie (traci ~0,3 s z 20 s) - kosmetyka.
- Konflikt z PvE: brak - dzikie spotkanie wymaga mainScreenReadyForWild (karta zamknieta), PvE laduje/zwalnia wildPmd tylko w startBattle;
  PvP zwalnia w pvpFxBattleEnd (takze przy forceStop -> STOPPING).
- sd=0: PmdMon::load zwraca false od razu (sdReady), thumbs.get -> nullptr (loaded=false) -> brak sprite'ow, bez crasha. Fallback
  "pusta scena" (brak miniatur tez z SD) - akceptowalne.
### 3 NVS w trakcie PvP
- pvpBattleBegin: jeden putBool (idempotentny). recordPvpQuit: jeden putUShort + remove. recordPvpMatch: pelny save() (komentarz w ino:628
  "NVS congela ~1s ambos cores") - wolany w onFinished po wyslaniu BYE; sesja w FINISHED lingeruje i retransmituje, wiec zamrozenie nie
  zrywa wyniku. finishPeerQuit tak samo (ABORT juz odebrany).
- flushSave zablokowany gdy pvpAppActive (ino:632) - poprawnie; pendingSave z pet.tick czeka do konca PvP.
- Pozostale pelne save() mozliwe w walce: ensureDailyGoals (zmiana dnia o polnocy), applyWalk/noteDailyGoal. Pojedyncze ~1 s << 10 s
  PEER_TIMEOUT. Niekrytyczne; do obserwacji w tescie HW (walka przez polnoc).
### 4 AGENTS.md / higiena
- ASCII: diff czysty (2 nie-ASCII w ino to stare komentarze spoza diffu). git diff --check czysty. Pliki <= 167 linii (theme.h 245 = tabela stalych).
- Hardkody: wymiary/kolory/czasy w pvp_ui_theme.h i pvp_config.h. Drobne: literal "vs %s %s" w pvp_ui_result.cpp:30 (nie tlumaczony).
- Testy: tests/pvp_ui_text_tests.cpp testChordTable (tabela geometrii ze spec C.0) i testCenter (pvpCenterX/pvpTextWidth) to testy ukladu
  -> do usuniecia wg reguly globalnej; zostawic testFit (bezpieczenstwo bufora) i ewentualnie 2 asercje "poza kolem = 0".
  pvp_ui_battle_logic_tests: testClock (arytmetyka zawijania) ok; testMineLine/FoeLine - mapowanie na teksty, graniczne (zostawic).
- Gitlinki: brak. Worktree agentow nadal na dysku: .claude/worktrees/agent-a94918148586e39ed (fec8024) i agent-aea0f4e9c798cdabb
  (3118d70) - oba zmergowane, do `git worktree remove` + usuniecia galezi worktree-agent-*.
- Zakres PR: oprocz PvP zawiera nakladke zasilania (9ee2b1e, b366b87) i pomiar T4.0 (db5c523) - opisac w PR.
### 5 Werdykt
BLOKUJACE: brak (kod). make test EXIT=0 potwierdzone.
NIEKRYTYCZNE do zrobienia przed testem HW albo w T4.4:
- N1 pvp_ui_battle.cpp pvpUiDrawBattle: sprite'y po paskach HP -> duzy Pokemon zaslania HP/liczby/"-N"/chipy (gora sprite'a do y~86,
  spec rezerwuje 150..256). Rysowac sprite'y przed pvpBattleDrawSides albo ograniczyc skale (wysokosc <= 106 px).
- N2 pvp_ui_ended.cpp:76: ENDED bez blokady tapu jak RESULT (PVP_RESULT_LOCK_MS). OK 118..348 x 300..350 = pod QUICK/HEAVY i STAY modala;
  ucieczka rywala/zerwanie lacza + tap w akcje = natychmiastowe wyjscie, gracz nie widzi "rywal uciekl, +N".
- N3 PREPARING ANULUJ (133..333 x 300..344) wypada na QUICK/HEAVY ekranu walki: tap tuz po BATTLE_START wybiera akcje (bez kary).
  Lokalna blokada wejscia ~300-800 ms po wejsciu w CHOOSE z PREPARING rozwiazuje N3 i spelnia spec C.5.
- N4 spec C.5 mowi 800 ms blokady akcji po ROUND_CONFIRMED, kod ma PVP_ROUND_LOCK_MS=250 (zgodnie z tabela E.2) - spec sprzeczny, ujednolicic.
- N5 RESULT OK 118..348 x 396..448 wychodzi poza kolo w dolnych rogach (brzeg przy x=118 to y~436).
- N6 testChordTable/testCenter do usuniecia (patrz 4). N7 z review linku: esp_now_del_peer, sprawdzenie set_channel, +76 KB flash.
Niezweryfikowane bez sprzetu: dotyk i strefy na kole (N3/N5, slop 7 px), czytelnosc i nakladanie sprite'ow (N1), czas wildPmd.load na
prawdziwej karcie i jego wplyw na pompe, czas petli przy renderze co 200 ms (drawGameScene + 2 sprite'y + flush), dzwiek ESP-NOW +
I2S (zaklocenia, SFX_GAME_START + chirp jednoczesnie), pobor pradu z radiem (light sleep wylaczony), zamrozenie NVS przy pelnym
save w walce, reboot w walce (-1 przy starcie), walka przez polnoc, brak karty SD na jednej z plytek.
