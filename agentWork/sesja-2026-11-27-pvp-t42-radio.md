---
STATUS: IN_PROGRESS
ZADANIE: T4.2 - walka PvP po ESP-NOW (radio, adapter Pet, UI strony 10, bot testowy)
AKTUALIZACJA: 2026-11-27
---

## Punkt wznowienia
Test UI na plytce, potem review Opus calosci i PR. (Dawny punkt: po raporcie agenta UI (worktree, dziennik agentWork/a-pvp-ui-t42.md): zintegrowac pvp_ui_*, dopisac pvp_app_instance.cpp i haki A.4 w TamaPoke.ino, build + test z botem na N16R8.

## Stan pracy
[x] pvp_espnow_link.h/.cpp (transport wg spec B)
[x] pvp_host_pet.h/.cpp (profil + ksiega na Pet)
[x] bot testowy tools/pvp_bot + tools/build_pvp_bot.sh (N16R8, ten sam rdzen PvpApp)
[x] test sprzetowy radia: pelna walka 18 rund Waveshare(bot) <-> N16R8(bot): wyniki komplementarne (W=0, N=2), brak desync, obie strony wrocily do SEARCHING
[x] review Opus transportu (agentWork/review-t42-link.md): brak blokujacych; poprawione: atomowy gInFlight, kontrola set_channel, sciezka OUT w build_pvp_bot.sh, tempo bota
[ ] DO ZROBIENIA z review: esp_now_del_peer przy koncu sesji (niekrytyczne), canFight tez przy wyzwaniu, +77 KB flash/+4.5 KB DRAM w release dopiero z hakami (merge razem z hakami A.4), test sprzetowy kary za ucieczke/forceStop/NVS w walce
[x] agent UI (pvp_strings 39 kluczy x 7 jezykow, pvp_ui_*; mutacje 9/9) zmergowany do branchu
[x] pvp_app_instance.cpp + haki A.4 w TamaPoke.ino, CARD_COUNT 10 (commit d74e5da); make test zielony; release 1.40 MB flash, DRAM 62 KB
[~] test sprzetowy UI z botem (Jarek: dotyk, strona karty 10 -> wyzwanie Botek); wymaga pelnego cyklu zasilania Waveshare (SD)
[ ] review Opus calosci, PR

## Decyzje
- D1-D11 wg rekomendacji specu (Jarek: "pomiary 1-plytkowe + D1-D11 wg rekomendacji")
- Drugi ESP32-S3 N16R8 = bot walki (Jarek: "mozesz uzywac drugiej plytki esp jako bota")
- Walka (T4.3) po T4.2 jako osobny PR; Jarek nie odpowiedzial na pytanie o zakres, zakladam rekomendacje

## Stan srodowisk
- Waveshare: firmware TamaPoke release z nakladka baterii, po flashu sd=0 do pelnego cyklu zasilania
- N16R8: bot (pvp_bot), port /dev/cu.usbmodem2101
- PR #10 (nakladka baterii) otwarty, czeka na ocene wygladu na ekranie
