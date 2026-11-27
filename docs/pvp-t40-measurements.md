# T4.0: procedura pomiaru PvP (dwie plytki)

Cel: zebrac liczby, od ktorych zalezy start T4.2 (radio ESP-NOW). Kod pomiarowy jest tylko w buildzie `TAMAPOKE_LOCAL_TEST`
(`pvp_diag_*.cpp`, hak w `handleSerial()` w `TamaPoke.ino`); build release go nie zawiera (rozmiar release bez zmian).
Spec: `docs/pvp-integration-spec.md` (A.3a, B.3-B.8). Opis w tym pliku jest bez polskich znakow (jak reszta docs/).

**Stan: kod skompilowany, nietestowany na plytce.** Wszystkie liczby ponizej trzeba dopiero zmierzyc.

## 1. Wgranie buildu testowego na dwie plytki

Potrzebne: dwie plytki Waveshare ESP32-S3-Touch-AMOLED-1.75 z wgranym juz Gen3 (karta SD ze sprite'ami nie jest wymagana do pomiarow,
ale niech bedzie wlozona jak na co dzien - SD_MMC jest jednym z wspolistniejacych podsystemow).

```bash
# kopia projektu do folderu o nazwie szkicu (bez .git, tests, web, tools, *.md)
mkdir -p /tmp/t40/TamaPoke
rsync -a --exclude .git --exclude .claude --exclude tests --exclude web --exclude tools --exclude agentWork --exclude docs --exclude '*.md' ./ /tmp/t40/TamaPoke/

arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
  --build-property "compiler.cpp.extra_flags=-DTAMAPOKE_LOCAL_TEST" \
  --build-property "compiler.c.extra_flags=-DTAMAPOKE_LOCAL_TEST" \
  --build-path /tmp/t40/build /tmp/t40/TamaPoke

# plytka 1 (port zalezy od systemu: ls /dev/cu.usbmodem*)
arduino-cli upload --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
  -p /dev/cu.usbmodemXXXX --input-dir /tmp/t40/build
```

- **Przed wgraniem zrobic kopie zapisu** (kazda plytka): lokalnie `python3 -m http.server 8000 --directory web`, w Chrome/Edge otworzyc
  `http://localhost:8000/release-gen3.html`, polaczyc plytke i kliknac "Download backup" (komenda `SAVEGET`, plik `.tamapoke-save`).
  Przywracanie: ten sam ekran, "Restore backup" (przed nadpisaniem pobiera kopie bezpieczenstwa).
- Nie uzywac `build.extra_flags` (nadpisuje flagi platformy). **Erase device wylaczone** - zapis gry ma przetrwac.
- Powtorzyc upload na drugiej plytce (inny port). Zapisac, ktora plytka to A, a ktora B.
- Build testowy ma `FW_VERSION` `1.36.2-hoenn-walk-local`.

### 1.1 Powrot do release Gen3 (1.36.2) po pomiarach

Obie plytki to Gen3 (386 gatunkow, partycje `app3M_fat9M_16MB`); wracamy do `FW_VERSION "1.36.2"` (`tools/build_web_gen3.sh`), nie do publicznego Gen2.
Bez kasowania urzadzenia - zapis zostaje. Dwie drogi:

1. Z tego samego drzewa, build release (to samo co w `tools/build_web_gen3.sh`: `-DTAMAPOKE_GEN3_RELEASE`):
   ```bash
   arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
     --build-property "compiler.cpp.extra_flags=-DTAMAPOKE_GEN3_RELEASE" --build-property "compiler.c.extra_flags=-DTAMAPOKE_GEN3_RELEASE" \
     --build-path /tmp/t40/build-release /tmp/t40/TamaPoke
   arduino-cli upload --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
     -p /dev/cu.usbmodemXXXX --input-dir /tmp/t40/build-release
   ```
2. Instalatorem `web/release-gen3.html` (manifest `web/manifest-gen3.json`, 1.36.2) przez lokalny serwer; **"Erase device" odznaczone**.
   Dziala tylko jesli pliki `web/firmware/tamapoke-1.36.2-*` sa w repo/na dysku.

Po powrocie do release: `NVSSTAT` nie istnieje, ale w NVS **zostana wpisy `phy` / `nvs.net80211`** utworzone przez WiFi w buildzie testowym
(ok. 67 wpisow na blob PHY wg D.3). To zamierzone i bezpieczne; sprawdzic `NVSSTAT` w buildzie testowym PRZED powrotem. Jesli cos poszlo nie tak: "Restore backup".
- Terminal szeregowy: dowolny monitor 115200 (`arduino-cli monitor -p <port> -c baudrate=115200`). Komendy konczy Enter (LF).
  Kazda komenda konczy sie linia `DONE` (poza przerwaniem). Komendy blokuja petle gry na czas dzialania (ekran stoi) - to zamierzone.

## 2. Komendy

| Komenda | Plytki | Co robi |
|---|---|---|
| `NVSSTAT` | 1 | stan NVS (used/free/available/total/erased) + liczba kluczy w kazdym namespace |
| `RADIOCYC` | 1 | pomiar czasu startu WiFi, 10 cykli start/stop radia z pomiarem sterty i NVS, ocena bramek |
| `PVPPING` / `PVPPING audio` | 2 | 1000 pakietow w 20 s w obie strony (broadcast + unicast), RSSI, % strat; wariant `audio` gra dzwiek w petli |
| `GEOM` | 1 | ekran kalibracyjny geometrii; Enter w monitorze zamyka (samoczynnie po 5 min). **Przed komenda obudzic ekran i ustawic pelna jasnosc** (komenda blokuje petle, wiec nie przywroci jasnosci ani nie wybudzi ekranu) |

Uwagi:
- `erased` w `NVSSTAT` jest **wyliczone** (`total - used - free`): struktura `nvs_stats_t` w core 3.3.10 nie ma takiego pola (spec zakladal pole).
  Przyrost `erased` miedzy pomiarami mowi, czy PHY/WiFi przepisuje wpisy przy kazdym starcie.
- Stale i progi sa w jednym miejscu: `pvp_diag_config.h`.

### 2.1 NVSSTAT i RADIOCYC (jedna plytka)

1. **Swiezy reset plytki** (przycisk RESET albo odlaczenie i podlaczenie USB). Pierwszy start WiFi po resecie jest "zimny"; tylko wtedy czas jest miarodajny.
2. `NVSSTAT` - zapisz wynik jako "przed WiFi" (jeszcze nic nie uruchamialo radia po tym resecie).
3. `RADIOCYC` zaraz po resecie - wypisuje: stan przed WiFi, 10 wierszy cykli (`start_ms`, `stop_ms`, `free/min/maxblock` w gorze i po stopie),
   `NVSSTAT` po 1. starcie i po 10 cyklach (z lista namespace'ow: czy pojawia sie `phy` i `nvs.net80211`), na koncu cztery bramki PASS/FAIL.
4. Powtorzyc `RADIOCYC` drugi raz (bez resetu) i `NVSSTAT` - drugi bieg pokazuje, czy `erased` dalej rosnie (przepisywanie bloba PHY).
5. Powtorzyc na drugiej plytce.

### 2.2 PVPPING (dwie plytki)

1. Obie plytki w odleglosci ok. 1 m. **Obie z otwartym monitorem szeregowym** (dwa terminale) albo jedna zasilana z ladowarki USB: plytka wpieta do
   komputera bez otwartego monitora moze blokowac w `Serial.print`. W fazach 1-3 komenda nie czeka na port (timeout 0), ale raport koncowy zobaczysz tylko tam, gdzie monitor jest otwarty.
2. Wpisac `PVPPING` na A, w ciagu 30 s na B. Plytki wymieniaja HELLO, potem wysylaja po 1000 ramek (parzyste = broadcast, nieparzyste = unicast,
   co 20 ms), potem komplet raportow. Kazda strona drukuje swoj raport (straty = pakiety, ktore *ona* odebrala od drugiej).
   W logu: `own mac`, `deviceId`, `peer mac`, `payload_id` vs `mac_id` (musza byc rowne: `ID_OK`).
3. Bieg bez audio: `PVPPING`. **Dla biegu z audio tryb dzwieku musi byc FULL** (ustawienia dzwieku na obu plytkach; efekt `SFX_BALL_BOUNCE`
   gra tylko w FULL). Komenda wypisuje `sound mode=3` i przy innym trybie konczy sie `ERR sound mode must be full`. Bieg z audio: `PVPPING audio` na obu plytkach (dzwiek efektu co ok. 350 ms przez caly pomiar).
   Podczas biegu z audio **sluchac trzaskow/zaklocen** i zapisac ocene (brak / slabe / wyrazne).
4. Przerwanie: dowolny znak w terminalu.
5. Powtorzyc w dwoch odleglosciach (ok. 1 m i ok. 5 m, najlepiej przez sciane) - to daje obraz RSSI i strat.
6. Zwrocic uwage, czy dwie plytki maja **rozne** `deviceId` (powinny, to koncowka MAC STA).

## 3. Progi bramek (T4.2 nie startuje, jesli ktoras nie przejdzie)

| Bramka | Prog | Skad wynik |
|---|---|---|
| wolne wpisy NVS po 10 cyklach | `available` >= 55 % z (`used` + `available`), czyli bez zarezerwowanej strony (ostrozniej niz `free` / `total`, ktore ja wlicza; budzet D.3: ok. 39 % zajete z ok. 504 uzytecznych) | `RADIOCYC`, linia `gate nvs_available` |
| najwiekszy wolny blok sterty po `start()` | >= 60 KB (61440 B) w kazdym cyklu | `RADIOCYC`, `gate max_block` |
| sterta cykl N vs cykl 1 | `free` PO `stop()` (po 100 ms pauzie) w cyklu N nie nizsze niz w cyklu 1 o wiecej niz 2048 B (wyciek); stan "up" tylko informacyjnie | `RADIOCYC`, `gate heap_leak` |
| pierwszy start radia | < 1,5 s [?] (tylko po swiezym resecie); mierzy caly `start()` (WiFi.mode(STA) + kanal + `esp_now_init` + peer), nie sam `WiFi.mode(STA)` | `RADIOCYC`, `gate first_start` |
| straty pakietow bez audio | < 5 % (lacznie broadcast + unicast) | `PVPPING`, `gate loss` |
| straty pakietow z audio | < 10 % oraz brak trzaskow | `PVPPING audio`, `gate loss` + sluch |
| identyfikatory | `deviceId` rozne na plytkach, `id_mismatch=0` | `PVPPING`, `gate ... ids` |

Dodatkowe pytania z review ESP32 (odpowiedz = wpis w tabeli wynikow):
- czy `esp_wifi_init` pisze do `nvs.net80211` mimo `WiFi.persistent(false)` (namespace na liscie po 1. starcie)?
- czy powstaje namespace `phy` i ile kluczy (blob kalibracji ok. 1,9 KB)?
- czy `erased` rosnie przy kazdym starcie (PHY przepisuje blob)?

## 4. Ekran GEOM

`GEOM` rysuje na pelnym ekranie: czerwony okrag r=225, zielony r=233, zolty r=240 wokol (233,233); azurowe linijki poziome na y=30, 60 i 430
(podzialka co 20 px, dluzsza kreska i etykieta x co 100 px); paski cyfr `0123456789...` rozmiarem 1 (pod y=30), 2 (pod y=60), 3 (nad y=430).
Zapisac:
- ktory okrag jest jeszcze w calosci widoczny (czerwony / zielony / zolty; r=240 wychodzi poza ekran, widac tylko fragmenty),
- na kazdej linijce: pierwsza i ostatnia w pelni widoczna cyfra paska (z lewej i z prawej krawedzi) - to szerokosc uzyteczna na tej wysokosci,
- czy cos jest przyciete przez zaokraglenie rogow obudowy (rama) mimo ze mieści sie w r=233.
Z tego liczymy `PVP_UI_EDGE_MARGIN` i poprawiamy tabele C.0 w specyfikacji.

## 5. Tabela wynikow (do wypelnienia)

Data: ____  Plytka A: ______ (deviceId ______)  Plytka B: ______ (deviceId ______)  Build: ______

### 5.1 NVS i pamiec (plytka A / plytka B)

| Pomiar | A | B | Prog |
|---|---|---|---|
| NVS przed WiFi: used / free / total / erased | | | - |
| NVS po 1. starcie: used / free / erased | | | - |
| NVS po 10 cyklach: used / free / erased, avail % (i free %) | | | avail % >= 55 |
| namespace'y po cyklach (czy `phy`, `nvs.net80211`; keys) | | | - |
| przyrost `erased` miedzy 1. i 2. biegiem RADIOCYC | | | informacyjnie |
| pierwszy `start()` radia [ms] (swiezy reset; caly start(), nie sam WiFi.mode) | | | < 1500 |
| kolejny start [ms] (srednia z cykli 2-10) | | | informacyjnie |
| stop [ms] (srednia) | | | informacyjnie |
| sterta w gore, cykl 1: free / min / maxblock | | | maxblock >= 61440 |
| sterta po stop(), cykl 1 i cykl 10: free | | | free10 >= free1 - 2048 |
| sterta w gore, cykl 10: free / min / maxblock | | | informacyjnie |
| najmniejszy maxblock z 10 cykli | | | >= 61440 |
| bramki RADIOCYC (4x PASS/FAIL) | | | wszystkie PASS |

### 5.2 PVPPING

| Pomiar | Bez audio, ~1 m | Z audio, ~1 m | Bez audio, ~5 m | Z audio, ~5 m |
|---|---|---|---|---|
| A odebrala broadcast (rx/oczekiwane, strata %) | | | | |
| A odebrala unicast (rx/oczekiwane, strata %) | | | | |
| B odebrala broadcast | | | | |
| B odebrala unicast | | | | |
| RSSI avg / min / max (A, B) | | | | |
| tx fail / send_cb_fail / rx_queue_drops (A, B) | | | | |
| id_mismatch (A, B) | | | | |
| trzaski audio (brak / slabe / wyrazne) | - | | - | |
| werdykt (PASS/FAIL, prog 5 % / 10 %) | | | | |

Tozsamosc: `deviceId` A = ______, B = ______ (rozne? ___); `payload_id == mac_id` na obu (ID_OK? ___).

### 5.3 GEOM

| Element | Wynik |
|---|---|
| okrag w pelni widoczny (225 / 233 / 240) | |
| y=30, rozmiar 1: pierwsza / ostatnia widoczna cyfra | |
| y=60, rozmiar 2: pierwsza / ostatnia widoczna cyfra | |
| y=430, rozmiar 3: pierwsza / ostatnia widoczna cyfra | |
| przyciecia przy ramie obudowy | |

### 5.4 Decyzja

Wszystkie bramki PASS? ____ . Jesli nie: ktora, i co proponujesz (np. decyzja o SD, ROADMAP poz. 5/7; kanal 1 vs inny; moc TX).
Wynik skopiowac do `agentWork/` i do sekcji "Wyniki T4.0" w `docs/pvp-integration-spec.md`.
