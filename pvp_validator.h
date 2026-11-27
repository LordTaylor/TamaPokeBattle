#pragma once

#include <stdint.h>

#include "battle.h"
#include "pvp_packet.h"
#include "pvp_stat_range.h"

enum PvpStatsVerdict : uint8_t {
  PVP_STATS_OK = 0,
  PVP_STATS_BAD_DEX,
  PVP_STATS_BAD_LEVEL,
  PVP_STATS_BAD_SHINY,
  PVP_STATS_HP_NONZERO,  // hp must be 0: the engine derives max HP itself
  PVP_STATS_BAD_RANGE,   // generic range (pvpValidateStats)
  PVP_STATS_BAD_TYPE,    // types differ from DEX_TBL
  PVP_STATS_BAD_STAT,    // atk/def/spe outside the legal band of species and level
};

// Legal atk/def/spe for a base stat at a level: base*gene/100 + level + training.
PvpStatRange pvpStatRange(uint8_t base, uint8_t level);

// Engine-side check of the opponent's READY (species, level, hp == 0, types, stat bands).
PvpStatsVerdict pvpCheckOpponentReady(const PvpReadyPayload &ready);

// Same check for the LOCAL READY (hp is ignored): lets the host report a clear error instead of
// being aborted by every rival with BAD_STATS (e.g. a hand-edited backup).
PvpStatsVerdict pvpCheckLocalReady(const PvpReadyPayload &ready);

// READY payload for the wire: hp is always 0.
PvpReadyPayload pvpReadyFromStats(int16_t dex, bool shiny, const BattleStats &stats);
