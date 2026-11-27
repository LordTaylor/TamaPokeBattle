#pragma once

#include <stdint.h>

#include "pvp_ledger.h"
#include "pvp_link.h"
#include "pvp_match.h"
#include "pvp_profile.h"
#include "pvp_pump.h"
#include "pvp_session.h"
#include "pvp_ui_model.h"

// PvP application controller (pure logic, no UI, no radio): owns the session, the
// match and the phase machine; talks to the world through PvpLink / PvpProfile / PvpLedger.
// Call everything from loop() only.
//
// Per battle exactly ONE of ledger.recordMatch / recordQuit / battleEnd follows
// battleBegin (battleOpen_). A local quit sets localQuit_ BEFORE session.abort(), because
// ABORTED(USER) looks the same for our own and for the rival's abort; in that branch
// battleEnd() is never called (it would clear the flag and the penalty would be lost).
//
// File split: pvp_app.cpp (lifecycle, user actions, timers), pvp_app_events.cpp (session events).
class PvpApp : private PvpEventSink {
 public:
  PvpApp(PvpLink &link, PvpProfile &profile, PvpLedger &ledger)
      : link_(link), profile_(profile), ledger_(ledger), pump_(link, session_) {}

  // Radio on + discovery. False: refused (model().deny) or radio failure (model().endReason).
  bool open(uint32_t nowMs);
  void update(uint32_t nowMs);  // safe while idle

  // --- User actions (false = not valid in the current phase) ---
  bool challengePeer(uint8_t index, uint32_t nowMs);
  bool respondChallenge(bool accept, uint32_t nowMs);
  bool submitAction(uint8_t action, uint32_t nowMs);
  bool quit(uint32_t nowMs);   // confirmed escape from a battle (penalty)
  bool leave(uint32_t nowMs);  // cancel / OK: back to the card page via STOPPING
  void touch(uint32_t nowMs) { lastInteract_ = nowMs; }
  // Staging gate (default PVP_BATTLE_ENABLED): when off, BATTLE_START ends the session cleanly.
  void setBattleEnabled(bool enabled) { battleEnabled_ = enabled; }
  // Safety net when the card is closed by another path: abort (+ quit penalty in battle), radio off.
  void forceStop(uint32_t nowMs);

  // --- Queries ---
  const PvpUiModel &model() const { return model_; }
  const PvpMatch &match() const { return match_; }
  const PvpSession &session() const { return session_; }
  bool active() const { return model_.phase != PVP_UI_IDLE; }
  bool radioOn() const { return link_.running(); }
  bool keepAwake() const { return model_.phase >= PVP_UI_CHALLENGED && model_.phase <= PVP_UI_WAIT_FINAL; }
  bool canLeaveNow(uint32_t nowMs) const { return session_.canLeave(nowMs); }

 private:
  // Events (pvp_app_events.cpp)
  void onEvent(const PvpEvent &event, uint32_t nowMs) override;
  void onAccepted(const PvpEvent &e, uint32_t nowMs);
  void onOpponentReady(const PvpEvent &e, uint32_t nowMs);
  void onBattleStart(uint32_t nowMs);
  void onRoundReady(const PvpEvent &e, uint32_t nowMs);
  void onAborted(uint8_t reason, uint32_t nowMs);
  void onFinished(uint32_t nowMs);
  void finishQuit(uint32_t nowMs);
  void finishPeerQuit(uint32_t nowMs);
  void endBattleWithoutResult(PvpEndReason reason, uint32_t nowMs);
  void backToSearch(PvpNotice notice, uint32_t nowMs);

  // Lifecycle (pvp_app.cpp)
  void setPhase(PvpPhase phase, uint32_t nowMs);
  void setEnded(PvpEndReason reason, uint32_t nowMs);
  void beginStop(uint32_t nowMs);
  void finishStop();
  void tickTimers(uint32_t nowMs);

  PvpLink &link_;
  PvpProfile &profile_;
  PvpLedger &ledger_;
  PvpSession session_;
  PvpPump pump_;
  PvpMatch match_;
  PvpUiModel model_;
  PvpReadyPayload localReady_;

  bool localQuit_ = false;
  bool rejected_ = false;
  bool battleOpen_ = false;
  bool battleEnabled_ = PVP_BATTLE_ENABLED;
  bool roundLocked_ = false;
  uint32_t lastInteract_ = 0;
  uint32_t roundConfirmedMs_ = 0;
  uint32_t stopSinceMs_ = 0;
};
