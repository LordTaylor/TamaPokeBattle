# Reguly walki PvP (silnik symetryczny, PVP_RULES_VERSION = 3)

Kod: `pvp_battle.h` / `pvp_battle.cpp`, wspolne wzory i stale w `battle_core.h` (te same co PvE w `battle.cpp`).
Silnik jest czysty i deterministyczny (bez Arduino, `random()`, `millis()`): dwa urzadzenia z tymi samymi danymi
wejsciowymi (statystyki A i B, akcje, `luckRoll` kazdej tury) dostaja identyczny stan i identyczny `pvpStateHash`.

Wersja regul (`PVP_RULES_VERSION`) trzeba podniesc przy KAZDEJ zmianie reguly, stalej albo wzoru wplywajacej na wynik.
Wysylamy ja w handshake i wchodzi do hasha stanu: rozne wersje = rozne hashe od pierwszej tury.

## Wejscie i walidacja

- `pvpValidateStats`: level 1..100, atk/def/spe 1..999, hp 0 (pochodne z level/def, jak w grze) albo 1..`PVP_HP_MAX` (999),
  type1/type2 0..18. Zakresy biorą się z `pvp_config.h` (jedno zrodlo prawdy); w silniku stale nazywaja sie `PVP_RULE_*` i sa z niego przypisane.
- `pvpBegin(out, A, B)` zwraca `PvpError`; przy blednych statystykach `out` jest wyzerowany (`PVP_ERR_STATS`).
  `pvpStep` na nierozpoczetym/wyzerowanym runtime (maxHp == 0) zwraca `PVP_ERR_STATS`, nie cichy remis.
- `pvpStep(battle, actionA, actionB, roll)`: akcje to `uint8_t`; akcja >= 5 -> `PVP_ERR_ACTION`, roll > 99 ->
  `PVP_ERR_LUCK`. Przy bledzie stan nie zmienia sie, wynik ma `error != PVP_OK` (wolajacy powinien ABORT-owac).
- `luckRoll` MUSI byc 0..99. Z dowolnego slowa 32-bit: `pvpRollFrom(x)` = `x % 100`.

## Strony i luck

- Strona A = urzadzenie o nizszym `deviceId` (decyduje wolajacy), B = drugie.
- Na ture jest jeden wspolny `luckRoll` 0..99. Luck strony A = `roll`, strony B = `99 - roll` (jak wrog w PvE).
- Luck atakujacego steruje wariancja obrazen (90..110%) i rzutem uniku z predkosci. Luck broniacego steruje akcja DODGE.
- Symetria: zamiana A<->B przy zamianie statystyk, akcji i `roll -> 99 - roll` daje lustrzany wynik (test).

## Tura

1. Obie akcje rozstrzygane jednoczesnie na podstawie stanu sprzed tury.
2. Najpierw leczenie (REST), potem obrazenia obu stron; HP nie schodzi ponizej 0 ani nie przekracza maxHp.
3. `round++` po kazdej rozegranej turze. Koniec: czyjes HP = 0 albo `round >= 20`.
4. `pvpStep` po zakonczeniu walki nic nie zmienia (zwraca `battleEnded`).

Atakuje tylko strona, ktora wybrala ATTACK / QUICK / HEAVY. DODGE i REST nie zadaja obrazen
(roznica wzgledem PvE, gdzie wrog zawsze kontratakuje).

## Wzory

- Obrazenia: `battle_core::turnDamage` (jak PvE): atk, def, level, typ, luck; moc ATTACK 100%, QUICK 85%,
  HEAVY 125%, counter x1.5, limit 35% maxHp celu. Liczone w uint32 z clampem (bez zawijania uint16).
- Typy: `battleTypeEffectPct(typ1 atakujacego, typy celu)`. maxHp: ta sama logika co `beginBattleRuntime`.
- Unik z predkosci (cel nie wybral DODGE): szansa 10%, 18% gdy spe celu > spe atakujacego (scisle wieksze),
  clamp 0..45; **w PvP bez modyfikatorow akcji** (PvE: QUICK -9, HEAVY +22). Trafienie uniku:
  `(luckAtakujacego + spe_celu/2) % 100 < szansa`.
- DODGE broniacego: blok gdy luck broniacego < prog, wtedy broniacy dostaje counter; inaczej cios / 3 (min 1).
  Prog zalezy od "licznika grzania" DODGE (`dodgeHeat`): +2 za kazdy efektywny DODGE (maks. 6), -1 za kazda inna
  ture (min 0). Pasma: licznik 0 -> prog **65**, 1..2 -> **40**, 3 i wiecej -> **20**.
  QUICK atakujacego przebija DODGE: prog nie wiekszy niz **30**. Counter tylko gdy zablokowano realny atak.
  (Wersja 2 miala czysta serie zerowana kazda inna akcja; naprzemienne ATTACK/DODGE omijalo oslabianie.)
- Counter: nastepny trafiajacy cios tej strony x1.5, zuzywa sie przy trafieniu (blok/unik go nie zuzywa).
- REST: leczy **5% maxHp (min 3)**, nie wiecej niz brakuje; limit 2 uzycia na strone (PvE: 28%, min 6).
  **Nieudany REST (limit wyczerpany) zamienia sie w DODGE** (flaga `restFailed`, runda sie liczy, licznik grzania rosnie).
- Obrona wg wlasnej akcji (obrazenia otrzymane): REST x0.70, HEAVY x1.20, ATTACK x1.00,
  QUICK: x0.85 vs ATTACK, x0.92 vs HEAVY, x1.00 vs QUICK (lekka oslona dziala w pelni tylko przeciw zwyklemu atakowi).

## Uklad kamien-papier-nozyce

- HEAVY bije QUICK (QUICK traci oslone, HEAVY bije mocniej), QUICK bije DODGE (prog 30), DODGE bije HEAVY/ATTACK (blok 65%).
- ATTACK jest bezpiecznym wyborem srodka; REST to zasob pod koniec walki, nie zwykla akcja.

## Wynik

- HP = 0 u jednej strony: wygrywa druga. HP = 0 u obu: remis.
- Limit 20 rund: porownanie procentu HP krzyzowo `hpA * maxHpB` vs `hpB * maxHpA` (uint32, bez zaokraglen);
  wiekszy wygrywa, rowny = remis (zasada wlasciciela).
- Hash stanu (`pvpStateHash`, FNV-1a): `PVP_RULES_VERSION`, round, oraz dla obu stron: statystyki
  (hp, atk, def, spe, level, type1, type2), hp, maxHp, damageTotal, restUsesLeft, dodgeHeat, counterReady.

## Brak impasu

Dwa czyste DODGE/REST nie zadaja obrazen, wiec konczy sie to limitem rund i remisem przy rownym HP (z regul nie
da sie tego zmienic bez wymuszania ataku). Aktywna strona nie moze jednak "zawisnac": REST ma 2 uzycia i
leczy tylko 5%, licznik grzania DODGE obniza szanse bloku (65 -> 40 -> 20), a QUICK przebija DODGE.

## Macierz balansu (rowne statystyki: atk 70, def 55, spe 50, lvl 20; 20 000 walk na pare, roll losowy)

Format: wygrane A / wygrane B / remisy w %; `lim` = walki zakonczone limitem rund (hp obu > 0).
Strategie: ATK/QCK/HVY = zawsze ta akcja; ATK/DDG = 70% ATTACK, 30% DODGE; TURTLE = DODGE gdy prowadzi, inaczej ATTACK;
ATK+RST = ATTACK, REST gdy HP < 40% (jesli jest limit); MIX3 = losowo ATK/QCK/HVY; MIX4 = losowo ATK/QCK/HVY/DDG.

| A \ B | ATK | QCK | HVY | ATK/DDG | TURTLE | ATK+RST | MIX3 | MIX4 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ATK | 41/41/18 lim0 | 49/48/3 lim77 | 35/46/19 lim0 | 39/58/3 lim83 | 58/35/7 lim71 | 37/53/10 lim25 | 37/45/18 lim1 | 35/60/5 lim66 |
| QCK | 49/48/3 lim76 | 49/48/3 lim77 | 16/70/13 lim1 | 63/35/2 lim100 | 70/24/6 lim94 | 35/63/2 lim100 | 34/58/9 lim28 | 47/51/2 lim97 |
| HVY | 46/34/20 lim0 | 70/17/13 lim1 | 40/39/21 lim0 | 44/44/12 lim29 | 63/25/12 lim48 | 45/36/20 lim0 | 51/30/18 lim0 | 44/42/15 lim16 |
| ATK/DDG | 59/38/3 lim83 | 36/62/2 lim100 | 44/44/11 lim30 | 49/49/2 lim100 | 64/32/4 lim94 | 43/55/2 lim98 | 47/49/4 lim81 | 42/56/2 lim99 |
| TURTLE | 36/57/8 lim71 | 24/70/6 lim94 | 25/63/12 lim48 | 33/63/4 lim94 | 47/48/5 lim100 | 28/67/5 lim78 | 28/64/8 lim65 | 27/69/4 lim88 |
| ATK+RST | 53/37/10 lim25 | 62/36/2 lim100 | 36/45/20 lim0 | 55/43/2 lim98 | 67/28/5 lim78 | 49/49/3 lim93 | 47/41/12 lim24 | 50/48/2 lim94 |
| MIX3 | 45/37/18 lim1 | 57/34/9 lim28 | 30/51/19 lim0 | 49/48/3 lim81 | 64/28/8 lim64 | 42/47/12 lim23 | 41/41/18 lim1 | 43/51/6 lim59 |
| MIX4 | 61/34/5 lim66 | 51/47/2 lim97 | 43/43/14 lim15 | 56/42/2 lim99 | 68/28/4 lim87 | 48/50/2 lim94 | 51/43/6 lim60 | 49/49/2 lim96 |

Wnioski:
- Zadna czysta akcja nie dominuje: najwiekszy skos to kontra HEAVY vs QUICK (70/17) i QUICK vs TURTLE (70/24), czyli
  zamierzony uklad kamien-papier-nozyce; pozostale pary sa w przedziale ok. 35-65%.
- Poprzedni (v1) stan: QUICK bil ATTACK 85/6, QUICK vs QUICK konczylo sie w 100% limitem rund, ATK+REST (28% leczenia)
  wygrywalo ze zwyklym ATTACK praktycznie zawsze. Walka ma niska wariancje (kilka trafien), dlatego male zasoby
  (leczenie, szansa bloku) mocno przesuwaja wynik: stad REST 5% i prog DODGE 65%.
- QUICK vs QUICK: ok. 23% koncy sie KO, reszta limitem rund (rozstrzyga procent HP, remisy ok. 3%).
- Czysty DODGE/REST (nigdy nie atakuje) przegrywa 100% z kazdym, kto atakuje, a wzajemnie daje 100% limitu rund
  i remis - to nie strategia, tylko brak gry (pozostawione celowo, nie liczone jako dominacja).

## Znane wady / do strojenia

- Remis przy limicie rund wymuszony przez dwie bierne strony (DODGE vs DODGE).
- ATK vs HEAVY: HEAVY ma lekka przewage (46/35 przy rownych statystykach, remisy 19%); HEAVY vs HEAVY ma ok. 21% remisow
  (obaj padaja w tej samej turze przy limicie 35% maxHp na ture).
- Luck jednej tury jest skorelowany (A: roll, B: 99 - roll): gdy A ma dobry luck, B ma zly.
- Litery 85/70/120 itd. w PvE sa teraz nazwanymi stalami z `battle_core.h`; wartosci tylko PvP maja prefiks `PVP_` w `pvp_battle.h`.
- Wartosci strojenia (65/40/20, 30, 5%, 0.92) pochodza z symulacji na jednej parze rownych statystyk; warto powtorzyc
  na prawdziwych gatunkach (rozne spe/typy) przed ostateczna akceptacja.

## Symulacje v3 (licznik grzania) i znane ograniczenia

Symulator 12 strategii (ATK, QCK, HVY, DDG, ATK/DDG, TURTLE, ATK+RST, MIX4, ALT_AD, STALL, QTURT, STALL2), 20 000 walk
na pare, rowne statystyki L20 (70/55/50), L50 (150/120/100), L100 (300/250/200). "Najgorszy" = wynik strategii
przeciw jej najlepszej kontrze (1.0 = zawsze wygrywa, 0.5 = remis w sensie wartosci gry).

| Strategia (najgorsze zestawienie) | v2 L20 | v3 L20 | v2 L50 | v3 L50 | v2 L100 | v3 L100 |
| --- | --- | --- | --- | --- | --- | --- |
| ALT_AD (ATTACK/DODGE naprzemiennie) | 0.31 | 0.15 | 0.31 | 0.15 | 0.30 | 0.14 |
| STALL | 0.50 | 0.50 | 0.50 | 0.50 | 0.50 | 0.50 |
| TURTLE | 0.16 | 0.12 | 0.12 | 0.10 | 0.10 | 0.09 |
| ATK | 0.21 | 0.35 | 0.20 | 0.32 | 0.21 | 0.32 |
| HVY | 0.28 | 0.48 | 0.28 | 0.47 | 0.28 | 0.46 |
| QCK | 0.23 | 0.23 | 0.11 | 0.11 | 0.13 | 0.13 |

- ALT_AD (v2: 70-78% wygranych z ATK/HVY/TURTLE) w v3 przegrywa z kazda sensowna strategia (v3: 28/70 z ATK, 20/78 z HVY).
- ATK i HVY maja teraz najgorsze zestawienie 0.32-0.48 zamiast 0.20-0.28, wiec nie sa juz lamane przez DODGE-spam.
- Ostatnia kolumna wynikow v3 wskazuje, ze zamierzone kontry (HVY>QCK, QCK>TURTLE) zostaly bez zmian.

### Znane ograniczenie: STALL

STALL (atakuj, az wyjdziesz na prowadzenie w % HP, potem DODGE/REST i przeczekaj) jest nadal najsilniejsza strategia
(czysty rownowaga Nasha = STALL, wartosc 0.50 przeciw wszystkim, 64-90% z ATK/TURTLE). Przyczyna: limit 20 rund
rozstrzyga % HP, a przy wyzszych poziomach walki sa dluzsze: limit rund pada w ~40% walk ATK vs ATK na L50
i w 100% na L100 (zadna walka nie konczy sie KO). Prowadzenie + przeczekanie jest wtedy dominujace.

W tym PR nie przebudowujemy HP ani dlugosci walki. Rekomendacja do strojenia po testach na sprzecie (decyzja Jarka):
- cel 8-12 tur do KO (np. osobny wspolczynnik maxHp w PvP, rzedu polowy PvE; prototyp pokazal limit rund ~0%);
- tie-break dla podwojnego KO (przy cap 35% maxHp na ture remisy rosna do 20-30%);
- rozwazyc cap na ture i sile HEAVY po zmianie HP (HEAVY staje sie wtedy najlepsza akcja na L50/L100).

### Znane ograniczenie: podwojne liczenie obrony

`def` podnosi maxHp (`30 + level*5 + def`) i jednoczesnie obniza otrzymywane obrazenia. W efekcie tank 80/200 (atk/def)
wygrywa z "glass cannonem" 200/60 przy L50 w 98-99% walk, a przy limicie rund `def` jest kluczowy. Do strojenia razem
z wspolczynnikiem HP (np. oddzielny wplyw def na maxHp w PvP).
