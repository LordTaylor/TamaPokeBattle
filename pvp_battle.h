#pragma once

#include <stdint.h>

#include "battle.h"
#include "pvp_config.h"

// Symmetric two-action PvP battle engine. Pure and deterministic: no Arduino,
// no random(), no clock. Both devices feed identical inputs and get identical
// state. Side A is the device with the lower deviceId (decided by the caller).
// Rules: docs/pvp-battle-rules.md

// Bump on every change of a rule, constant or formula that affects results.
// Exchanged in the handshake and mixed into the state hash.
constexpr uint8_t PVP_RULES_VERSION = 3;

enum PvpSideId : uint8_t { PVP_SIDE_A = 0, PVP_SIDE_B = 1 };

enum PvpOutcome : uint8_t { PVP_OUTCOME_ONGOING = 0, PVP_OUTCOME_A_WINS, PVP_OUTCOME_B_WINS, PVP_OUTCOME_DRAW };

enum PvpViewOutcome : uint8_t { PVP_VIEW_ONGOING = 0, PVP_VIEW_WIN, PVP_VIEW_LOSS, PVP_VIEW_DRAW };

enum PvpError : uint8_t { PVP_OK = 0, PVP_ERR_STATS, PVP_ERR_ACTION, PVP_ERR_LUCK };

// Accepted input ranges: single source of truth is pvp_config.h (the wire format validates the same
// values). hp 0 means "derive from level/def" exactly like the game does; otherwise 1..PVP_RULE_HP_MAX.
constexpr uint8_t PVP_RULE_LEVEL_MIN = PVP_LEVEL_MIN;
constexpr uint8_t PVP_RULE_LEVEL_MAX = PVP_LEVEL_MAX;
constexpr uint16_t PVP_RULE_STAT_MIN = PVP_STAT_MIN;
constexpr uint16_t PVP_RULE_STAT_MAX = PVP_STAT_MAX;
constexpr uint16_t PVP_RULE_HP_MAX = PVP_HP_MAX;
constexpr uint8_t PVP_RULE_TYPE_MAX = PVP_TYPE_MAX;  // TYPE_FAIRY, 0 = none
constexpr uint8_t PVP_RULE_ACTION_COUNT = PVP_ACTION_COUNT;
constexpr uint8_t PVP_RULE_LUCK_MOD = PVP_LUCK_MOD;
constexpr uint8_t PVP_LUCK_MAX = PVP_RULE_LUCK_MOD - 1;  // luckRoll must be 0..99
static_assert(PVP_RULE_LUCK_MOD == 100, "luck rolls are 0..99 (A uses roll, B uses 99 - roll)");

// PvP-only tuning (shared formulas live in battle_core.h).
constexpr int8_t PVP_DODGE_QUICK_DELTA = 0;   // QUICK has no speed-dodge bonus (PvE: -9)
constexpr int8_t PVP_DODGE_HEAVY_DELTA = 0;   // HEAVY gets no speed-dodge penalty for the target (PvE: +22)
constexpr uint8_t PVP_REST_HEAL_MIN = 3;
constexpr uint8_t PVP_REST_HEAL_PCT = 5;    // PvE: 28; PvP fights are symmetric, so REST is a small resource
// DODGE "heat": +STEP per effective DODGE (saturating at MAX), -DECAY on every other turn (min 0).
// The block limit depends on the heat band: heat 0 -> band 0, 1..LOW_MAX -> band 1, above -> band 2.
constexpr uint8_t PVP_DODGE_HEAT_STEP = 2;
constexpr uint8_t PVP_DODGE_HEAT_DECAY = 1;
constexpr uint8_t PVP_DODGE_HEAT_MAX = 6;
constexpr uint8_t PVP_DODGE_HEAT_LOW_MAX = 2;
constexpr uint8_t PVP_DODGE_BANDS = 3;
// DODGE succeeds when luck < limit (index = heat band).
constexpr uint8_t PVP_DODGE_LIMIT_BY_BAND[PVP_DODGE_BANDS] = { 65, 40, 20 };
constexpr uint8_t PVP_DODGE_VS_QUICK_LIMIT = 30;  // QUICK pierces DODGE
// QUICK's light guard (x0.85) only fully works vs plain ATTACK.
constexpr uint8_t PVP_GUARD_QUICK_VS_QUICK_PCT = 100;
constexpr uint8_t PVP_GUARD_QUICK_VS_HEAVY_PCT = 92;

struct PvpSide {
  BattleStats stats;
  uint16_t hp;
  uint16_t maxHp;
  uint16_t damageTotal;  // damage this side has dealt so far
  uint8_t restUsesLeft;
  uint8_t dodgeHeat;   // DODGE heat, 0..PVP_DODGE_HEAT_MAX (see above)
  bool counterReady;
};

struct PvpRuntime {
  PvpSide side[2];
  uint8_t round;
};

// What happened to / because of one side during a turn.
struct PvpSideTurn {
  BattleAction action;      // effective action (failed REST becomes DODGE)
  uint16_t damageDealt;     // damage this side inflicted on the opponent
  uint16_t healed;
  uint8_t typePct;          // type effectiveness of this side's attack
  bool evaded;              // this side evaded an incoming attack by speed
  bool dodged;              // this side blocked an incoming attack with DODGE
  bool rested;
  bool guarded;             // incoming damage was modified by this side's action
  bool counterUsed;         // this side's attack was boosted by a counter
  bool counterGained;
  bool restFailed;          // REST requested without uses left
};

struct PvpTurnResult {
  PvpSideTurn side[2];
  bool battleEnded;
  PvpOutcome outcome;
  PvpError error;  // != PVP_OK: input rejected, state untouched
};

// Turn view relative to one side: mine = me, theirs = opponent.
struct PvpTurnView {
  PvpSideTurn mine;
  PvpSideTurn theirs;
  bool battleEnded;
  PvpViewOutcome outcome;
};

bool pvpValidateStats(const BattleStats &stats);
bool pvpValidAction(uint8_t action);
// Maps any 32-bit value (e.g. an RNG word or hash) to a valid luck roll 0..99.
uint8_t pvpRollFrom(uint32_t value);

// Rejects invalid stats (out is zeroed, PVP_ERR_STATS).
PvpError pvpBegin(PvpRuntime &out, const BattleStats &statsA, const BattleStats &statsB);
// Actions must be valid BattleAction values and luckRoll 0..99, otherwise the
// call is rejected and nothing changes (PVP_ERR_ACTION / PVP_ERR_LUCK).
PvpTurnResult pvpStep(PvpRuntime &battle, uint8_t actionA, uint8_t actionB, uint8_t luckRoll);
PvpOutcome pvpOutcome(const PvpRuntime &battle);
// FNV-1a over rules version, round and every field of both sides incl. stats.
uint32_t pvpStateHash(const PvpRuntime &battle);

// Luck of a side for a shared roll: A uses roll, B uses 99 - roll.
uint8_t pvpSideLuck(PvpSideId side, uint8_t luckRoll);

PvpSideId pvpOpponent(PvpSideId me);
const PvpSide &pvpMySide(const PvpRuntime &battle, PvpSideId me);
const PvpSide &pvpTheirSide(const PvpRuntime &battle, PvpSideId me);
PvpViewOutcome pvpViewOutcome(PvpOutcome outcome, PvpSideId me);
PvpTurnView pvpTurnView(const PvpTurnResult &turn, PvpSideId me);
