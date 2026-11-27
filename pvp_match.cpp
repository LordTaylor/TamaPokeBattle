#include "pvp_match.h"

bool PvpMatch::begin(const PvpReadyPayload &local, const PvpReadyPayload &opponent, bool localIsSideA) {
  ready_ = false;
  lastTurn_ = PvpTurnView{};
  side_ = localIsSideA ? PVP_SIDE_A : PVP_SIDE_B;
  BattleStats mine = pvpToBattleStats(local);
  BattleStats theirs = pvpToBattleStats(opponent);
  mine.hp = 0;
  theirs.hp = 0;
  const BattleStats &a = localIsSideA ? mine : theirs;
  const BattleStats &b = localIsSideA ? theirs : mine;
  if (pvpBegin(runtime_, a, b) != PVP_OK) return false;
  ready_ = true;
  return true;
}

bool PvpMatch::applyRound(uint16_t round, uint8_t actionA, uint8_t actionB, uint8_t luck, uint32_t &hashOut) {
  if (!ready_ || round != runtime_.round) return false;
  PvpTurnResult turn = pvpStep(runtime_, actionA, actionB, luck);
  if (turn.error != PVP_OK) return false;
  lastTurn_ = pvpTurnView(turn, side_);
  hashOut = pvpStateHash(runtime_);
  return true;
}

PvpViewOutcome PvpMatch::outcome() const {
  if (!ready_) return PVP_VIEW_ONGOING;
  return pvpViewOutcome(pvpOutcome(runtime_), side_);
}

PvpResult pvpResultFromView(PvpViewOutcome outcome) {
  switch (outcome) {
    case PVP_VIEW_WIN: return PVP_WIN;
    case PVP_VIEW_LOSS: return PVP_LOSS;
    case PVP_VIEW_DRAW: return PVP_DRAW;
    default: return PVP_ABORTED;
  }
}
