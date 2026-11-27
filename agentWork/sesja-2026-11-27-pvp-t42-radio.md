---
STATUS: IN_PROGRESS
ZADANIE: T4.2 - walka PvP po ESP-NOW (radio, adapter Pet, UI strony 10, bot testowy)
AKTUALIZACJA: 2026-11-27
---

## Punkt wznowienia
Po raporcie agenta UI (worktree, dziennik agentWork/a-pvp-ui-t42.md): zintegrowac pvp_ui_*, dopisac pvp_app_instance.cpp i haki A.4 w TamaPoke.ino, build + test z botem na N16R8.

## Stan pracy
[x] pvp_espnow_link.h/.cpp (transport wg spec B)
[x] pvp_host_pet.h/.cpp (profil + ksiega na Pet)
[x] bot testowy tools/pvp_bot + tools/build_pvp_bot.sh (N16R8, ten sam rdzen PvpApp)
[x] test sprzetowy radia: pelna walka 18 rund Waveshare(bot) <-> N16R8(bot): wyniki komplementarne (W=0, N=2), brak desync, obie strony wrocily do SEARCHING
[~] agent UI (pvp_strings, pvp_ui_*) w toku
[ ] pvp_app_instance.cpp, haki w TamaPoke.ino, CARD_COUNT 10
[ ] build release + test sprzetowy UI z botem, review Opus, PR

## Decyzje
- D1-D11 wg rekomendacji specu (Jarek: "pomiary 1-plytkowe + D1-D11 wg rekomendacji")
- Drugi ESP32-S3 N16R8 = bot walki (Jarek: "mozesz uzywac drugiej plytki esp jako bota")
- Walka (T4.3) po T4.2 jako osobny PR; Jarek nie odpowiedzial na pytanie o zakres, zakladam rekomendacje

## Stan srodowisk
- Waveshare: firmware TamaPoke release z nakladka baterii, po flashu sd=0 do pelnego cyklu zasilania
- N16R8: bot (pvp_bot), port /dev/cu.usbmodem2101
- PR #10 (nakladka baterii) otwarty, czeka na ocene wygladu na ekranie
