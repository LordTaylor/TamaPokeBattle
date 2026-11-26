#include "trainer.h"
#include <string.h>

static uint16_t fibonacci(uint8_t n) {
  uint16_t a = 1, b = 1;
  for (uint8_t i = 2; i < n; i++) {
    uint16_t t = (uint16_t)(a + b);
    a = b;
    b = t;
  }
  return b;
}

uint16_t trainerLevelCost(uint8_t level) {
  if (level < TRAINER_LEVEL_MIN || level >= TRAINER_LEVEL_MAX) return 0;
  uint8_t fibIdx = level < TRAINER_COST_FIB_CAP ? level : TRAINER_COST_FIB_CAP;
  uint16_t extra = level > TRAINER_COST_FIB_CAP ? (uint16_t)(TRAINER_COST_STEP * (level - TRAINER_COST_FIB_CAP)) : 0;
  return (uint16_t)(TRAINER_COST_BASE + fibonacci(fibIdx) + 1 + extra);
}

int8_t trainerPointsDelta(PvpResult result, uint8_t myLevel, uint8_t oppLevel) {
  switch (result) {
    case PVP_WIN: return (int8_t)(PVP_POINTS_WIN + (oppLevel > myLevel ? PVP_RANK_BONUS : 0));
    case PVP_DRAW: return PVP_POINTS_DRAW;
    case PVP_LOSS: return (int8_t)(PVP_POINTS_LOSS - (oppLevel < myLevel ? PVP_RANK_BONUS : 0));
    default: return 0;
  }
}

static void saturatingInc(uint16_t &v) {
  if (v < 0xFFFF) v++;
}

static void countResult(TrainerStats &s, PvpResult result) {
  if (result == PVP_WIN) saturatingInc(s.wins);
  else if (result == PVP_LOSS) saturatingInc(s.losses);
  else saturatingInc(s.draws);
  s.matchSeq++;
}

static uint8_t applyPoints(TrainerStats &s, int8_t delta) {
  if (s.level >= TRAINER_LEVEL_MAX) {
    s.progress = 0;
    return 0;
  }
  int32_t p = (int32_t)s.progress + delta;
  if (p < 0) p = 0;
  uint8_t gained = 0;
  while (s.level < TRAINER_LEVEL_MAX && p >= trainerLevelCost(s.level)) {
    p -= trainerLevelCost(s.level);
    s.level++;
    gained++;
  }
  s.progress = s.level >= TRAINER_LEVEL_MAX ? 0 : (uint16_t)p;
  return gained;
}

TrainerAward trainerApplyResult(TrainerStats &stats, PvpResult result, uint8_t oppLevel, bool pointsAllowed) {
  TrainerAward award;
  if (result != PVP_WIN && result != PVP_DRAW && result != PVP_LOSS) return award;
  countResult(stats, result);
  if (!pointsAllowed) {
    award.blocked = true;
    return award;
  }
  award.applied = true;
  award.pointsDelta = trainerPointsDelta(result, stats.level, oppLevel);
  award.levelsGained = applyPoints(stats, award.pointsDelta);
  return award;
}

bool trainerApplyQuitPenalty(TrainerStats &stats) {
  if (stats.level >= TRAINER_LEVEL_MAX || stats.progress == 0) return false;
  stats.progress = stats.progress > PVP_QUIT_PENALTY ? (uint16_t)(stats.progress - PVP_QUIT_PENALTY) : 0;
  return true;
}

void sanitizeTrainerStats(TrainerStats &stats) {
  if (stats.level < TRAINER_LEVEL_MIN) stats.level = TRAINER_LEVEL_MIN;
  if (stats.level > TRAINER_LEVEL_MAX) stats.level = TRAINER_LEVEL_MAX;
  uint16_t cost = trainerLevelCost(stats.level);
  if (cost == 0 || stats.progress >= cost) stats.progress = 0;
}

static const FarmEntry *farmFind(const FarmGuard &g, uint32_t opponentId) {
  for (const FarmEntry &e : g.entries)
    if (e.epoch && e.opponentId == opponentId) return &e;
  return nullptr;
}

bool farmGuardAllows(const FarmGuard &guard, uint32_t opponentId, uint32_t nowEpoch) {
  if (nowEpoch == 0) return false;  // RTC niewazny
  const FarmEntry *e = farmFind(guard, opponentId);
  if (!e) return true;
  if (nowEpoch < e->epoch) return false;  // zegar cofniety
  return nowEpoch - e->epoch >= FARM_COOLDOWN_SEC;
}

void farmGuardResync(FarmGuard &guard, uint32_t opponentId, uint32_t nowEpoch) {
  if (nowEpoch == 0) return;
  FarmEntry *e = const_cast<FarmEntry *>(farmFind(guard, opponentId));
  if (e && nowEpoch < e->epoch) e->epoch = nowEpoch;
}

void farmGuardRecord(FarmGuard &guard, uint32_t opponentId, uint32_t nowEpoch) {
  if (nowEpoch == 0) return;  // 0 oznacza pusty slot
  FarmEntry *slot = const_cast<FarmEntry *>(farmFind(guard, opponentId));
  if (!slot) {
    slot = &guard.entries[0];
    for (FarmEntry &e : guard.entries) {
      if (e.epoch == 0) { slot = &e; break; }
      if (e.epoch < slot->epoch) slot = &e;
    }
  }
  slot->opponentId = opponentId;
  slot->epoch = nowEpoch;
}

void pvpHistoryPush(PvpHistoryEntry *entries, const char *nick, PvpResult result, uint8_t oppLevel, uint32_t seq) {
  for (int i = PVP_HISTORY_SIZE - 1; i > 0; i--) entries[i] = entries[i - 1];
  PvpHistoryEntry e;
  memset(&e, 0, sizeof(e));
  if (nick) strncpy(e.nick, nick, PVP_NICK_MAX);
  e.result = (uint8_t)result;
  e.oppLevel = oppLevel;
  e.seq = seq;
  entries[0] = e;
}

void sanitizePvpHistory(PvpHistoryEntry *entries) {
  for (int i = 0; i < PVP_HISTORY_SIZE; i++) {
    PvpHistoryEntry &e = entries[i];
    e.nick[PVP_NICK_MAX] = 0;
    if (e.seq == 0 || e.result > PVP_LOSS) {
      memset(&e, 0, sizeof(e));
      e.result = PVP_ABORTED;
      continue;
    }
    if (e.oppLevel < TRAINER_LEVEL_MIN) e.oppLevel = TRAINER_LEVEL_MIN;
    if (e.oppLevel > TRAINER_LEVEL_MAX) e.oppLevel = TRAINER_LEVEL_MAX;
  }
}
