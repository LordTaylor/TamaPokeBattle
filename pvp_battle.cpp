#include "pvp_battle.h"
#include "battle_core.h"

namespace {
static_assert(PVP_DODGE_HEAT_MAX > PVP_DODGE_HEAT_LOW_MAX, "heat must be able to reach band 2");
static_assert(PVP_DODGE_BANDS == 3, "band selection below assumes three bands");
constexpr uint32_t FNV_OFFSET = 2166136261u;
constexpr uint32_t FNV_PRIME = 16777619u;
constexpr uint8_t BYTE_BITS = 8;
constexpr uint8_t PERCENT = 100;

struct Hit {
  uint16_t damage;
  bool evaded;
  bool blocked;
  bool counterUsed;
  uint8_t guardPct;  // 0 = no modifier applied
};

uint8_t sideIndex(PvpSideId id) { return (uint8_t)id; }

BattleAction effectiveAction(const PvpSide &side, BattleAction requested, bool &restFailed) {
  restFailed = requested == BATTLE_REST && side.restUsesLeft == 0;
  return restFailed ? BATTLE_DODGE : requested;
}

uint8_t dodgeChancePct(const PvpSide &target, const PvpSide &attacker, BattleAction attack) {
  int chance = target.stats.spe > attacker.stats.spe ? battle_core::DODGE_FAST_TARGET_PCT
                                                      : battle_core::DODGE_BASE_PCT;
  if (attack == BATTLE_ATTACK_QUICK) chance += PVP_DODGE_QUICK_DELTA;
  else if (attack == BATTLE_ATTACK_HEAVY) chance += PVP_DODGE_HEAVY_DELTA;
  if (chance < 0) chance = 0;
  if (chance > battle_core::DODGE_MAX_PCT) chance = battle_core::DODGE_MAX_PCT;
  return (uint8_t)chance;
}

uint16_t scaledMin1(uint16_t damage, uint8_t pct) {
  uint16_t scaled = (uint16_t)((uint32_t)damage * pct / PERCENT);
  return scaled == 0 ? 1 : scaled;
}

uint8_t incomingGuardPct(BattleAction defenderAction, BattleAction attackerAction) {
  switch (defenderAction) {
    case BATTLE_REST: return battle_core::GUARD_REST_PCT;
    case BATTLE_ATTACK_QUICK:
      if (attackerAction == BATTLE_ATTACK) return battle_core::GUARD_QUICK_PCT;
      return attackerAction == BATTLE_ATTACK_HEAVY ? PVP_GUARD_QUICK_VS_HEAVY_PCT
                                                   : PVP_GUARD_QUICK_VS_QUICK_PCT;
    case BATTLE_ATTACK_HEAVY: return battle_core::GUARD_HEAVY_PCT;
    default: return 0;
  }
}

uint8_t dodgeLimitFor(const PvpSide &defender, BattleAction attackerAction) {
  const uint8_t heat = defender.dodgeHeat;
  uint8_t band = heat == 0 ? 0 : (heat <= PVP_DODGE_HEAT_LOW_MAX ? 1 : 2);
  if (band >= PVP_DODGE_BANDS) band = PVP_DODGE_BANDS - 1;  // never index past the table
  uint8_t limit = PVP_DODGE_LIMIT_BY_BAND[band];
  if (attackerAction == BATTLE_ATTACK_QUICK && limit > PVP_DODGE_VS_QUICK_LIMIT) {
    limit = PVP_DODGE_VS_QUICK_LIMIT;
  }
  return limit;
}

// Resolves one attack from pre-turn state. luckAtt drives damage and the
// speed-dodge roll; luckDef drives the DODGE action of the defender.
Hit resolveHit(const PvpSide &att, const PvpSide &def, BattleAction attAct,
               BattleAction defAct, uint8_t luckAtt, uint8_t luckDef) {
  Hit hit = {};
  uint16_t raw = battle_core::turnDamage(att.stats, def.stats, def.maxHp, luckAtt,
                                         att.counterReady, battle_core::attackPowerPct(attAct));
  if (defAct == BATTLE_DODGE) {
    if (luckDef < dodgeLimitFor(def, attAct)) {
      hit.blocked = true;
      return hit;
    }
    hit.damage = raw > battle_core::FAILED_DODGE_DIVISOR - 1 ? raw / battle_core::FAILED_DODGE_DIVISOR : 1;
  } else {
    uint8_t chance = dodgeChancePct(def, att, attAct);
    hit.evaded = ((uint16_t)luckAtt + def.stats.spe / 2) % PERCENT < chance;
    if (hit.evaded) return hit;
    hit.guardPct = incomingGuardPct(defAct, attAct);
    hit.damage = hit.guardPct ? scaledMin1(raw, hit.guardPct) : raw;
  }
  hit.counterUsed = att.counterReady;
  return hit;
}

uint16_t restHeal(const PvpSide &side) {
  uint16_t heal = (uint16_t)((uint32_t)side.maxHp * PVP_REST_HEAL_PCT / PERCENT);
  if (heal < PVP_REST_HEAL_MIN) heal = PVP_REST_HEAL_MIN;
  uint16_t missing = side.maxHp - side.hp;
  return heal > missing ? missing : heal;
}

void fillSideTurn(PvpSideTurn &out, const PvpSide &self, const PvpSide &other,
                  BattleAction action, bool restFailed, const Hit &mine, const Hit &incoming) {
  out.action = action;
  out.restFailed = restFailed;
  out.typePct = battleTypeEffectPct(self.stats.type1, other.stats.type1, other.stats.type2);
  out.rested = action == BATTLE_REST;
  out.counterUsed = mine.counterUsed;
  out.evaded = incoming.evaded;
  out.dodged = incoming.blocked;
  out.counterGained = incoming.blocked;
  out.guarded = incoming.guardPct != 0 && incoming.damage > 0;
}

void applyDamage(PvpSide &target, uint16_t damage, PvpSide &dealer, uint16_t &dealt) {
  dealt = damage > target.hp ? target.hp : damage;
  target.hp -= dealt;
  dealer.damageTotal += dealt;
}

void updateDodgeHeat(PvpSide &self, BattleAction action) {
  if (action == BATTLE_DODGE) {
    uint8_t heat = self.dodgeHeat + PVP_DODGE_HEAT_STEP;
    self.dodgeHeat = heat > PVP_DODGE_HEAT_MAX ? PVP_DODGE_HEAT_MAX : heat;
  } else if (self.dodgeHeat > 0) {
    self.dodgeHeat = self.dodgeHeat > PVP_DODGE_HEAT_DECAY ? self.dodgeHeat - PVP_DODGE_HEAT_DECAY : 0;
  }
}

void applySide(PvpSide &self, const Hit &mine, bool gainedCounter) {
  if (mine.counterUsed) self.counterReady = false;
  if (gainedCounter) self.counterReady = true;
}

bool isOver(const PvpRuntime &b) {
  return b.side[0].hp == 0 || b.side[1].hp == 0 || b.round >= battle_core::MAX_TURN_ROUNDS;
}

void mixByte(uint32_t &h, uint8_t v) { h = (h ^ v) * FNV_PRIME; }

void mix16(uint32_t &h, uint16_t v) {
  mixByte(h, (uint8_t)(v & 0xFF));
  mixByte(h, (uint8_t)(v >> BYTE_BITS));
}
}  // namespace

uint8_t pvpSideLuck(PvpSideId side, uint8_t luckRoll) {
  uint8_t luck = battle_core::clampLuck(luckRoll);
  return side == PVP_SIDE_A ? luck : (uint8_t)(PVP_LUCK_MAX - luck);
}

bool pvpValidateStats(const BattleStats &s) {
  if (s.level < PVP_RULE_LEVEL_MIN || s.level > PVP_RULE_LEVEL_MAX) return false;
  if (s.hp > PVP_RULE_HP_MAX) return false;  // 0 = derived
  const uint16_t core[3] = { s.atk, s.def, s.spe };
  for (uint16_t v : core) {
    if (v < PVP_RULE_STAT_MIN || v > PVP_RULE_STAT_MAX) return false;
  }
  return s.type1 <= PVP_RULE_TYPE_MAX && s.type2 <= PVP_RULE_TYPE_MAX;
}

bool pvpValidAction(uint8_t action) { return action < PVP_RULE_ACTION_COUNT; }

uint8_t pvpRollFrom(uint32_t value) { return (uint8_t)(value % PVP_RULE_LUCK_MOD); }

PvpError pvpBegin(PvpRuntime &battle, const BattleStats &statsA, const BattleStats &statsB) {
  battle = PvpRuntime{};
  if (!pvpValidateStats(statsA) || !pvpValidateStats(statsB)) return PVP_ERR_STATS;
  const BattleStats *stats[2] = { &statsA, &statsB };
  for (uint8_t i = 0; i < 2; i++) {
    PvpSide &s = battle.side[i];
    s.stats = *stats[i];
    s.maxHp = battle_core::turnMaxHp(s.stats);
    s.hp = s.maxHp;
    s.restUsesLeft = battle_core::REST_USES_PER_BATTLE;
  }
  return PVP_OK;
}

PvpOutcome pvpOutcome(const PvpRuntime &battle) {
  const PvpSide &a = battle.side[0];
  const PvpSide &b = battle.side[1];
  if (a.hp == 0 && b.hp == 0) return PVP_OUTCOME_DRAW;
  if (b.hp == 0) return PVP_OUTCOME_A_WINS;
  if (a.hp == 0) return PVP_OUTCOME_B_WINS;
  if (battle.round < battle_core::MAX_TURN_ROUNDS) return PVP_OUTCOME_ONGOING;
  uint32_t lhs = (uint32_t)a.hp * b.maxHp;  // hpA/maxA vs hpB/maxB, cross-multiplied
  uint32_t rhs = (uint32_t)b.hp * a.maxHp;
  if (lhs == rhs) return PVP_OUTCOME_DRAW;
  return lhs > rhs ? PVP_OUTCOME_A_WINS : PVP_OUTCOME_B_WINS;
}

PvpTurnResult pvpStep(PvpRuntime &battle, uint8_t actionA, uint8_t actionB, uint8_t luckRoll) {
  PvpTurnResult turn = {};
  if (!pvpValidAction(actionA) || !pvpValidAction(actionB)) {
    turn.error = PVP_ERR_ACTION;
    return turn;
  }
  if (luckRoll > PVP_LUCK_MAX) {
    turn.error = PVP_ERR_LUCK;
    return turn;
  }
  if (battle.side[0].maxHp == 0 || battle.side[1].maxHp == 0) {
    turn.error = PVP_ERR_STATS;  // never started (or pvpBegin failed): no silent draw
    return turn;
  }
  if (isOver(battle)) {
    turn.battleEnded = true;
    turn.outcome = pvpOutcome(battle);
    return turn;
  }
  const PvpSide pre[2] = { battle.side[0], battle.side[1] };
  bool failed[2] = {};
  BattleAction act[2] = {
    effectiveAction(pre[0], (BattleAction)actionA, failed[0]),
    effectiveAction(pre[1], (BattleAction)actionB, failed[1]),
  };
  uint8_t luck[2] = { pvpSideLuck(PVP_SIDE_A, luckRoll), pvpSideLuck(PVP_SIDE_B, luckRoll) };
  Hit hits[2] = {};  // hits[i] = attack made BY side i
  for (uint8_t i = 0; i < 2; i++) {
    if (!battle_core::isAttackAction(act[i])) continue;
    hits[i] = resolveHit(pre[i], pre[1 - i], act[i], act[1 - i], luck[i], luck[1 - i]);
  }
  battle.round++;
  for (uint8_t i = 0; i < 2; i++) {
    PvpSide &self = battle.side[i];
    if (act[i] == BATTLE_REST) {
      self.restUsesLeft--;
      turn.side[i].healed = restHeal(pre[i]);
      self.hp += turn.side[i].healed;
    }
  }
  for (uint8_t i = 0; i < 2; i++) {
    applyDamage(battle.side[1 - i], hits[i].damage, battle.side[i], turn.side[i].damageDealt);
  }
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t o = 1 - i;
    uint16_t healed = turn.side[i].healed;
    uint16_t dealt = turn.side[i].damageDealt;
    fillSideTurn(turn.side[i], pre[i], pre[o], act[i], failed[i], hits[i], hits[o]);
    turn.side[i].healed = healed;
    turn.side[i].damageDealt = dealt;
    applySide(battle.side[i], hits[i], hits[o].blocked);
    updateDodgeHeat(battle.side[i], act[i]);
  }
  turn.battleEnded = isOver(battle);
  turn.outcome = pvpOutcome(battle);
  return turn;
}

static void mixStats(uint32_t &h, const BattleStats &st) {
  mix16(h, st.hp);
  mix16(h, st.atk);
  mix16(h, st.def);
  mix16(h, st.spe);
  mixByte(h, st.level);
  mixByte(h, st.type1);
  mixByte(h, st.type2);
}

uint32_t pvpStateHash(const PvpRuntime &battle) {
  uint32_t h = FNV_OFFSET;
  mixByte(h, PVP_RULES_VERSION);
  mixByte(h, battle.round);
  for (uint8_t i = 0; i < 2; i++) {
    const PvpSide &s = battle.side[i];
    mixStats(h, s.stats);
    mixByte(h, s.dodgeHeat);
    mix16(h, s.hp);
    mix16(h, s.maxHp);
    mix16(h, s.damageTotal);
    mixByte(h, s.restUsesLeft);
    mixByte(h, s.counterReady ? 1 : 0);
  }
  return h;
}

PvpSideId pvpOpponent(PvpSideId me) { return me == PVP_SIDE_A ? PVP_SIDE_B : PVP_SIDE_A; }

const PvpSide &pvpMySide(const PvpRuntime &battle, PvpSideId me) { return battle.side[sideIndex(me)]; }

const PvpSide &pvpTheirSide(const PvpRuntime &battle, PvpSideId me) {
  return battle.side[sideIndex(pvpOpponent(me))];
}

PvpViewOutcome pvpViewOutcome(PvpOutcome outcome, PvpSideId me) {
  switch (outcome) {
    case PVP_OUTCOME_DRAW: return PVP_VIEW_DRAW;
    case PVP_OUTCOME_A_WINS: return me == PVP_SIDE_A ? PVP_VIEW_WIN : PVP_VIEW_LOSS;
    case PVP_OUTCOME_B_WINS: return me == PVP_SIDE_B ? PVP_VIEW_WIN : PVP_VIEW_LOSS;
    default: return PVP_VIEW_ONGOING;
  }
}

PvpTurnView pvpTurnView(const PvpTurnResult &turn, PvpSideId me) {
  PvpTurnView view = {};
  view.mine = turn.side[sideIndex(me)];
  view.theirs = turn.side[sideIndex(pvpOpponent(me))];
  view.battleEnded = turn.battleEnded;
  view.outcome = pvpViewOutcome(turn.outcome, me);
  return view;
}
