#pragma once
#include <stdint.h>

// Logika poziomu trenera: czysta, bez zaleznosci od Arduino/NVS.

#define TRAINER_LEVEL_MIN 1
#define TRAINER_LEVEL_MAX 50

#define PVP_POINTS_WIN 3
#define PVP_POINTS_DRAW 2
#define PVP_POINTS_LOSS 1
#define PVP_QUIT_PENALTY 1   // ucieczka z walki: -punkty dla uciekajacego (poziom nie spada)
#define PVP_RANK_BONUS 2   // wygrana z wyzszym poziomem: +; przegrana z nizszym: -

#define TRAINER_COST_BASE 10
#define TRAINER_COST_FIB_CAP 9    // fib(min(L, cap))
#define TRAINER_COST_STEP 5       // dodatek za kazdy poziom powyzej capa

#define FARM_GUARD_SLOTS 4
#define FARM_COOLDOWN_SEC 3600UL

#define PVP_HISTORY_SIZE 5
#define PVP_NICK_MAX 10
#define PVP_NICK_UNKNOWN "???"  // nick przeciwnika bez zadnej litery

enum PvpResult : uint8_t { PVP_WIN, PVP_DRAW, PVP_LOSS, PVP_ABORTED };

struct TrainerStats {
  uint8_t level = TRAINER_LEVEL_MIN;
  uint16_t progress = 0;  // punkty w biezacym poziomie; na poziomie max zawsze 0
  uint16_t wins = 0, losses = 0, draws = 0;  // saturacja na 65535
  uint32_t matchSeq = 0;  // +1 za kazdy rozliczony mecz poza ABORTED
};

struct TrainerAward {
  int8_t pointsDelta = 0;     // nominalna zmiana punktow (przed obcieciem progress do 0)
  uint8_t levelsGained = 0;
  bool applied = false;       // true gdy punkty byly dozwolone i mecz nie byl ABORTED
  bool blocked = false;       // mecz rozliczony (W/L/D), ale punkty zablokowane przez FarmGuard; ABORTED daje false
};

struct FarmEntry {
  uint32_t opponentId = 0;
  uint32_t epoch = 0;  // 0 = pusty slot
};

struct FarmGuard {
  FarmEntry entries[FARM_GUARD_SLOTS];
};

struct PvpHistoryEntry {
  char nick[PVP_NICK_MAX + 1] = "";
  uint8_t result = PVP_ABORTED;
  uint8_t oppLevel = 0;
  uint32_t seq = 0;  // 0 = pusty wpis
};

uint16_t trainerLevelCost(uint8_t level);  // koszt awansu z level na level+1; 0 dla level >= max
int8_t trainerPointsDelta(PvpResult result, uint8_t myLevel, uint8_t oppLevel);
TrainerAward trainerApplyResult(TrainerStats &stats, PvpResult result, uint8_t oppLevel, bool pointsAllowed);
// Kara za ucieczke z walki: progress = max(0, progress - PVP_QUIT_PENALTY); poziom bez zmian,
// na poziomie max bez efektu. Nie rusza W/L/D ani matchSeq. Zwraca true, gdy progress sie zmienil.
bool trainerApplyQuitPenalty(TrainerStats &stats);
void sanitizeTrainerStats(TrainerStats &stats);

// Blokuje, gdy nowEpoch == 0 (brak wiarygodnego czasu), gdy zegar jest cofniety wzgledem wpisu
// albo gdy od ostatnich przyznanych punktow minelo mniej niz FARM_COOLDOWN_SEC.
bool farmGuardAllows(const FarmGuard &guard, uint32_t opponentId, uint32_t nowEpoch);
// Zegar cofniety wzgledem wpisu: epoch = nowEpoch (cooldown startuje od nowa). Wolac przed farmGuardAllows.
void farmGuardResync(FarmGuard &guard, uint32_t opponentId, uint32_t nowEpoch);
void farmGuardRecord(FarmGuard &guard, uint32_t opponentId, uint32_t nowEpoch);

// Najnowszy wpis na indeksie 0; najstarszy wypada.
void pvpHistoryPush(PvpHistoryEntry *entries, const char *nick, PvpResult result, uint8_t oppLevel, uint32_t seq);
void sanitizePvpHistory(PvpHistoryEntry *entries);
