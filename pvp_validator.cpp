#include "pvp_validator.h"

#include "dex.h"
#include "pvp_battle.h"

static_assert(PVP_DEX_MAX == DEX_COUNT, "wire dex limit must match the Pokedex");

PvpStatRange pvpStatRange(uint8_t base, uint8_t level) {
  PvpStatRange r;
  r.lo = (uint16_t)((uint16_t)base * PVP_GENE_MIN / 100 + level);
  r.hi = (uint16_t)((uint16_t)base * PVP_GENE_MAX / 100 + level + PVP_TRAINING_MAX);
  return r;
}

static bool inRange(uint16_t v, PvpStatRange r) { return v >= r.lo && v <= r.hi; }

PvpStatsVerdict pvpCheckOpponentReady(const PvpReadyPayload &ready) {
  if (ready.dex < PVP_DEX_MIN || ready.dex > PVP_DEX_MAX) return PVP_STATS_BAD_DEX;
  if (ready.level < PVP_LEVEL_MIN || ready.level > PVP_LEVEL_MAX) return PVP_STATS_BAD_LEVEL;
  if (ready.shiny > 1) return PVP_STATS_BAD_SHINY;
  if (ready.hp != 0) return PVP_STATS_HP_NONZERO;
  if (!pvpValidateStats(pvpToBattleStats(ready))) return PVP_STATS_BAD_RANGE;
  const DexEntry &d = DEX_TBL[ready.dex];
  if (ready.type1 != d.type1 || ready.type2 != d.type2) return PVP_STATS_BAD_TYPE;
  if (!inRange(ready.atk, pvpStatRange(d.bAtk, ready.level)) ||
      !inRange(ready.def, pvpStatRange(d.bDef, ready.level)) ||
      !inRange(ready.spe, pvpStatRange(d.bSpe, ready.level))) {
    return PVP_STATS_BAD_STAT;
  }
  return PVP_STATS_OK;
}

PvpStatsVerdict pvpCheckLocalReady(const PvpReadyPayload &ready) {
  PvpReadyPayload copy = ready;
  copy.hp = 0;
  return pvpCheckOpponentReady(copy);
}

PvpReadyPayload pvpReadyFromStats(int16_t dex, bool shiny, const BattleStats &stats) {
  PvpReadyPayload r;
  r.dex = dex < 0 ? 0 : (uint16_t)dex;
  r.shiny = shiny ? 1 : 0;
  r.level = stats.level;
  r.hp = 0;
  r.atk = stats.atk;
  r.def = stats.def;
  r.spe = stats.spe;
  r.type1 = stats.type1;
  r.type2 = stats.type2;
  return r;
}
