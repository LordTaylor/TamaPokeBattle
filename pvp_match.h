#pragma once

#include <stdint.h>

#include "pvp_battle.h"
#include "pvp_packet.h"
#include "trainer.h"

// One PvP battle seen from the local side: wraps the lockstep engine (pvp_battle.h).
// Side A is the device with the lower deviceId.
class PvpMatch {
 public:
  // hp of both READY payloads is ignored (zeroed): the engine derives max HP.
  // False when the statistics are rejected by the engine.
  bool begin(const PvpReadyPayload &local, const PvpReadyPayload &opponent, bool localIsSideA);
  bool ready() const { return ready_; }
  // `round` must be the engine's current round. False (state untouched) on a bad
  // round, action or luck. hashOut = state hash after the step.
  bool applyRound(uint16_t round, uint8_t actionA, uint8_t actionB, uint8_t luck, uint32_t &hashOut);
  PvpViewOutcome outcome() const;
  const PvpRuntime &runtime() const { return runtime_; }
  const PvpTurnView &lastTurn() const { return lastTurn_; }
  PvpSideId side() const { return side_; }

 private:
  PvpRuntime runtime_ = {};
  PvpTurnView lastTurn_ = {};
  PvpSideId side_ = PVP_SIDE_A;
  bool ready_ = false;
};

PvpResult pvpResultFromView(PvpViewOutcome outcome);  // ONGOING -> PVP_ABORTED
