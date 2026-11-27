---
STATUS: COMPLETE
ZADANIE: T4.2 czesc UI PvP: pvp_strings, pvp_ui_theme/text/bridge/ui.h, renderery strony karty i wyszukiwania (tylko nowe pliki, bez ruszania .ino i pvp_app*)
AKTUALIZACJA: 2026-11-27, make clean && make test exit 0; mutacje 9/9 zabite; firmware kompiluje i linkuje (probe)
---

## Punkt wznowienia
Sesja nadrzedna: dodac pvp_app_instance.cpp + hooki w TamaPoke.ino wg sekcji "Wymagane od sesji nadrzednej", potem test sprzetowy (spec G.2 pkt 2-6, 14-16, 20-21).

## Stan pracy
[x] pvp_strings.def/.h/.cpp (39 kluczy T4.2, 7 jezykow, static_assert na literalach) + test w tests/i18n_tests.cpp
[x] pvp_ui_theme.h, pvp_ui_text.h/.cpp + tests/pvp_ui_text_tests.cpp (cieciwa = tabela C.0, przycinanie)
[x] pvp_ui_bridge.h (audyt .ino: wszystkie symbole maja zewnetrzne wiazanie)
[x] pvp_ui.h (hooki A.4 + PvpApp &pvpAppInstance(), pvpProfileInstance(), pvpUiTrack)
[x] renderery: pvp_ui_card.cpp, pvp_ui_search.cpp, pvp_ui_challenge.cpp, pvp_ui_ended.cpp, pvp_ui_app.cpp (dispatch, tap, swipe), pvp_ui_effects.cpp (dzwieki, wybudzenie, toasty), pvp_ui_toast, pvp_ui_draw
[x] make test (exit 0), mutacje, kompilacja arduino-cli (Gen3 release, core 3.3.10): 790 898 B bez probe, 819 226 B z probe wymuszajacym linkowanie
[x] commity lokalne

## Decyzje podjete
- D1-D11 = rekomendacje spec | zlecenie | G.3
- Podzial renderow na kilka plikow zamiast jednego pvp_ui_search.cpp | brak monolitow | AGENTS.md
- Faza walki w T4.2: UI konczy mecz (leave) w PREPARING, gdy dostalo poprawne READY rywala (stala pvpui::BATTLE_UI_READY = false), zeby flaga walki nie zostala ustawiona i ucieczka nie kosztowala -1 | quit() po BATTLE_START daje kare | spec C.6, E.3
- Klucz STARTING z F.2 pominiety | radio startuje synchronicznie, brak ekranu | -
- %u w tescie dlugosci = 2 cyfry, %s = 10 znakow | spec F.1

## Sprawdzone zrodla
- docs/pvp-integration-spec.md (0, A.2-A.5, C.0-C.12, E, F, G.3)
- arduino-cli copy sketchu kopiuje .def (probe zlinkowana)

## Ustalenia
- Wlasciwosc: pvp_ui_*.cpp sa kompilowane przez arduino-cli (kazdy .cpp w katalogu szkicu), linker odrzuca niewolane sekcje; z probe (scratchpad, nie w repo) linkuje sie z gfx/pet/cardDirty/screenOff/lastInteract/stepsToEncounter/lockTouchBrief/flushStampedFrame z .ino.
- Wyscig: OPPONENT_READY i BATTLE_START moga trafic do jednego PvpApp::update(); wtedy guard w pvpUiTrack jest spozniony i model jest w CHOOSE (flaga walki ustawiona). Ekran-zaslepka "PvP wkrotce" + UCIEKAJ (quit, kara -1). Zalecenie: sesja nadrzedna w T4.2 niech zatrzyma PvpApp::onBattleStart (np. stala), albo zaakceptowac.
- Nie zweryfikowane bez sprzetu: geometria na plytce, dotyk, dzwieki, wybudzenie ekranu, radio.

## Otwarte pytania
- Patrz raport koncowy.
