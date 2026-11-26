#pragma once

#include <stdint.h>

#include "battle.h"

// Shared building blocks of the turn-based battle rules. PvE (battle.cpp) and
// PvP (pvp_battle.cpp) use the same formulas and constants so both modes stay
// in balance. PvP-only tuning lives in pvp_battle.h.
namespace battle_core {

constexpr uint8_t MAX_TURN_ROUNDS = 20;
constexpr uint8_t REST_USES_PER_BATTLE = 2;
constexpr uint8_t DODGE_LUCK_LIMIT = 85;  // luck below this: dodge succeeds
constexpr uint8_t FAILED_DODGE_DIVISOR = 3;

constexpr uint8_t POWER_QUICK_PCT = 85;
constexpr uint8_t POWER_HEAVY_PCT = 125;
constexpr uint8_t COUNTER_PCT = 150;
constexpr uint8_t TURN_CAP_PCT = 35;  // max share of the target's maxHp per hit

constexpr uint8_t REST_HEAL_PCT = 28;
constexpr uint8_t REST_HEAL_MIN = 6;
constexpr uint8_t GUARD_REST_PCT = 70;    // damage taken while resting
constexpr uint8_t GUARD_QUICK_PCT = 85;   // damage taken after QUICK
constexpr uint8_t GUARD_HEAVY_PCT = 120;  // damage taken after HEAVY

constexpr uint8_t DODGE_BASE_PCT = 10;
constexpr uint8_t DODGE_FAST_TARGET_PCT = 18;  // target spe > attacker spe
constexpr int8_t DODGE_QUICK_DELTA = -9;
constexpr int8_t DODGE_HEAVY_DELTA = 22;
constexpr uint8_t DODGE_MAX_PCT = 45;

uint8_t clampLuck(uint8_t luck);
bool isAttackAction(BattleAction action);
uint8_t attackPowerPct(BattleAction action);
uint16_t turnMaxHp(const BattleStats &stats);
uint16_t turnDamage(const BattleStats &attacker, const BattleStats &defender,
                    uint16_t defenderMaxHp, uint8_t luck, bool counter, uint8_t powerPct);

}  // namespace battle_core
