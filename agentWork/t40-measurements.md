---
STATUS: COMPLETE
ZADANIE: T4.0 - diagnostyka pomiarowa PvP (NVSSTAT, PVPPING, RADIOCYC, GEOM) pod TAMAPOKE_LOCAL_TEST + procedura pomiaru dla Jarka
AKTUALIZACJA: 2026-11-27 (po kompilacjach i testach)
---

## Punkt wznowienia
Kod gotowy i zacommitowany na feat/pvp-t40-measurements (bez push/PR). Nastepny krok: Jarek mierzy wg docs/pvp-t40-measurements.md na dwoch plytkach; wyniki do sekcji "Wyniki T4.0" spec.

## Stan pracy
- [x] branch feat/pvp-t40-measurements z origin/main
- [x] lektura spec A.3a/B, sygnatury API sprawdzone w core 3.3.10
- [x] baseline rozmiaru: release 788034 B flash / 35812 B DRAM; local-test (origin/main) 789590 / 35812
- [x] pvp_in_packet.h + pvp_config.h (PVP_WIFI_CHANNEL, PVP_RX_QUEUE_LEN) + tests/pvp_in_packet_tests.cpp (+blok w Makefile)
- [x] pvp_diag_* (NVSSTAT, PVPPING, RADIOCYC, GEOM), haki w .ino (include + dispatch w handleSerial, oba pod ifdef)
- [x] docs/pvp-t40-measurements.md
- [x] release po zmianach: 788034 / 35812 (identyczne); local-test: 1363139 / 59332
- [x] make test (rm -rf build): exit 0; mutacje pvpInPacketFrom (len, magic) wykryte
- [ ] test na plytce - NIE wykonany (brak sprzetu)

## Decyzje
- nvs_stats_t (core 3.3.10) nie ma pola erased | erased = total-used-free (pochodne) | nvs.h:594-601
- RSSI w strukturze diagnostycznej PvpDiagRx, PvpInPacket zostaje wg A.2 | nie rozszerzam kontraktu T4.1 | spec A.2
- Protokol PVPPING: ramki z PVP_MAGIC + znacznik 0xD0, wiec przechodza produkcyjny pvpInPacketFrom | test filtra w warunkach realnych
- Komendy blokuja loop() (jak SAVEGET uzywaja setTxTimeoutMs); po komendzie markUiDirty() w .ino
- GEOM zamykany Enterem z monitora (bez zalezności od touch/.ino)

## Sprawdzone zrodla
- nvs.h, esp_now.h, WiFiGeneric.h/.cpp w core 3.3.10; Arduino_GFX.h (kolory RGB565_*, bez makr WHITE/RED)
- spec docs/pvp-integration-spec.md (A.3a, B.1-B.8) w glownym checkoucie

## Ustalenia
- Stos radia: local-test +573 KB flash, +23.5 KB DRAM wzgledem local-test bez diag (zgodne ze spec: ok. +570 KB / +22 KB)
- Release identyczny bajt w bajt co do rozmiarow (WiFi nie jest linkowane)

## Otwarte pytania
- Wszystkie liczby (NVS, sterta, czas startu, straty, audio, geometria) do zmierzenia na plytkach

## Poprawki po review Opusa (2026-11-27)
- [x] PVPPING audio: wypisuje audioMode(), przy != FULL `ERR sound mode must be full` (SFX_BALL_BOUNCE ma SFX_MIN_MODE=FULL, audio.cpp:179)
- [x] Serial timeout 0 w fazach 1-3, 1500 ms tylko na raport (po stop() radia)
- [x] RADIOCYC leak z `down.freeHeap` po pauzie 100 ms
- [x] NVS gate na available/(used+available): total i free wliczaja zarezerwowana strone (630 vs ok. 504 uzytecznych wg D.3), wiec free/total jest optymistyczne; available/(used+available) jest ostrozniejsze i spojne z budzetem D.3
- [x] drobne: loss `<`, ABORTED w raporcie, `line == "PVPPING audio"`, start() przy gRunning wypelnia MAC, assert(!xPortInIsrContext()) w onRecv
- [x] docs: powrot do release Gen3 1.36.2 (arduino-cli -DTAMAPOKE_GEN3_RELEASE / release-gen3.html bez erase), backup SAVEGET przez release-gen3.html, phy/nvs.net80211 zostaja, GEOM jasnosc, rsync --exclude .claude, tryb FULL, first_start = caly start()
- Bramki: make test exit 0; release 788034/35812 bez zmian; local-test 1363831/59332
