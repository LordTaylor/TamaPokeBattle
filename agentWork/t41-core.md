---
STATUS: COMPLETE
ZADANIE: T4.1 - rdzen PvP testowalny na hoscie (PvpMatch, PvpPump, PvpApp, walidator, odznaki, filtr rx, interfejsy) + testy hostowe
AKTUALIZACJA: 2026-11-27, kod + testy gotowe, mutacje 14/15 zabite
---

## Punkt wznowienia
Review Opus, potem PR (nie robiony w tym przebiegu). Nastepny etap: T4.2 (radio, UI, pvp_strings). Wyniki: make test exit 0, mutacje 15/15 zabite, firmware 789 934 B.

## Stan pracy
[x] czytanie spec (A, B.5, D, E, G.3) i kodu T3
[x] interfejsy + pvp_in_packet
[x] pvp_validator, pvp_match, pvp_badges (+ Pet: tbadge, badgeEligible)
[x] pvp_pump, pvp_ui_model, pvp_versions, pvp_app*
[x] testy + Makefile (bloki dopisane na koncu; make test zielone, exit 0)
[~] mutacje: 14/15 zabite, przezyla "validator: widen range" (test uzywal tej samej funkcji) -> dodane niezalezne granice, do ponownego sprawdzenia
[ ] firmware, commit

## Decyzje podjete
- D1-D11: rekomendacje spec jako domyslne | polecenie | G.3
- pvp_strings / i18n_tests NIE w tym przebiegu (UI, poza zakresem briefu) | brief | -
- PvpLink dostal 6. metode inFlight() (domyslnie 0) | STOPPING czeka na flush wg B.6 | A.2
- Pump: sink dostaje nowMs; zdarzenia po jednym, przed kazdym sendAll() | gwarancja "pakiety przed recordMatch" | E.3
- PvpApp::quit() niedozwolone w WAIT_FINAL; forceStop dozwolone | C.6 / E.3 | -
- ABORTED(USER) przed bitwa: CHALLENGED -> SEARCHING + notice EXPIRED; CHALLENGING -> SEARCHING + DECLINED | spec milczy | -
- Pet::recordPvpMatch(..., bool badgeEligible = false): domyslnie bez odznaki | najbezpieczniej | D.2
- seq odznaki podnoszony ponad max istniejacych (odporny na utrate tseq) | odpornosc | D.1
- Blob odznak serializowany pole po polu (240 B, bez paddingu), zla dlugosc = pusty pierscien | D.1 | -

## Poprawki po review Opusa (review-t41-core.md)
- W1 forceStop: guard STOPPING, beginStop zamiast finishStop (flush <= 200 ms); testy: STOPPING -> IDLE, 1 linkStop, ABORT x3 przed recordQuit, wielokrotne wolanie nie przesuwa terminu
- W3 match_ = PvpMatch() w open() i onAccepted; test: nowa sesja bez dziedziczenia silnika (mutacja samego onAccepted rownowazna - open() tez resetuje; zostaje obrona w glab)
- D1 sanitize przenumerowuje seq 1..count; record sanituje przy top == UINT32_MAX; D2 badgeBlob = {} + wynik getBytes; D3 PVP_ROUND_LOCK_MS 250 (E.2), PVP_RESULT_LOCK_MS 800 osobno (spec C.5 ma 800 ms dla rund - sprzecznosc z E.2, wybralem 250 wg polecenia); D6 pvp_event_sink.h, pvp_stat_range.h, pvp_deny_reason.h (enumy w pvp_ui_model.h zostaja wg A.2); D7 constexpr PVP_BADGE_KEY; D8 pvpCheckLocalReady + test
- D4 test ABORTED(USER) w PREPARING/CHALLENGED/CHALLENGING; D5 asymetria PREPARING: rywal juz w BATTLE (dostal nasz READY), my jeszcze w PREPARING -> jego ucieczka = jego -1, my ENDED(PEER_QUIT) bez walkowera; rzadkie, akceptowalne
- Wyniki: make test exit 0; mutacje nowych sciezek 5/6 zabite (rownowazna: reset match_ w onAccepted); firmware 790 002 B, DRAM 36 068 B
- REBASE na origin/main dopiero po "T4.0 zmergowany" (konflikt: pvp_in_packet.h, pvp_config.h, tests/Makefile)

## Sprawdzone zrodla
- docs/pvp-integration-spec.md (repo glowne, nie w worktree): A.2, A.6, B.5, D.1-D.2, E.1-E.4, G.3

## Ustalenia
- PvpSession::abort() sam emituje ABORTED(USER) (abortLocal) - wlasne i zdalne USER nierozroznialne
- rejectOpponent emituje ABORTED(BAD_STATS)
- match_.ready() zostaje true po poprzednim meczu, wiec rejected_ jest wymagane (test: drugi mecz po pierwszym)

## Otwarte pytania
- pvp_strings.def/i18n_tests: robic w T4.2?
