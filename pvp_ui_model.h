#pragma once

#include <stdint.h>

#include "pvp_packet.h"
#include "pvp_peer_info.h"
#include "pvp_profile.h"
#include "trainer.h"

// Phase order matters: keep-awake covers CHALLENGED..WAIT_FINAL.
enum PvpPhase : uint8_t {
  PVP_UI_IDLE = 0,
  PVP_UI_SEARCHING,
  PVP_UI_CHALLENGING,
  PVP_UI_CHALLENGED,
  PVP_UI_PREPARING,
  PVP_UI_CHOOSE,        // round open, no action chosen yet
  PVP_UI_WAIT_OPP,      // local action submitted
  PVP_UI_WAIT_CONFIRM,  // round resolved locally, waiting for the hash confirmation
  PVP_UI_WAIT_FINAL,    // battle over locally, waiting for the last hash
  PVP_UI_RESULT,
  PVP_UI_ENDED,         // finished without a result (see endReason)
  PVP_UI_STOPPING,
};

enum PvpEndReason : uint8_t {
  PVP_END_NONE = 0,
  PVP_END_PEER_QUIT,   // rival left (before the battle: no points; in battle: walkover win, see result)
  PVP_END_YOU_QUIT,
  PVP_END_LINK,        // silence / timeout
  PVP_END_NO_REPLY,
  PVP_END_UPDATE,      // different protocol version
  PVP_END_SYNC,        // desync
  PVP_END_PROTO,
  PVP_END_STATS,       // statistics rejected (either side)
  PVP_END_RADIO_FAIL,
  PVP_END_IDLE,        // search screen left after PVP_SEARCH_IDLE_MS
};

enum PvpNotice : uint8_t {
  PVP_NOTICE_NONE = 0,
  PVP_NOTICE_DECLINED,
  PVP_NOTICE_EXPIRED,
  PVP_NOTICE_VERSION,
};

// Everything the renderers read; no game logic in the UI.
struct PvpUiModel {
  PvpPhase phase = PVP_UI_IDLE;
  PvpEndReason endReason = PVP_END_NONE;
  PvpNotice notice = PVP_NOTICE_NONE;
  PvpDenyReason deny = PVP_DENY_NONE;  // why open() was refused
  uint8_t peerCount = 0;
  PvpPeerInfo opponent;                // from CHALLENGE_RECEIVED / ACCEPTED
  PvpReadyPayload oppReady;
  uint16_t round = 0;
  uint32_t roundStartMs = 0;           // UI countdown only; the session decides timeouts
  uint8_t myAction = 0;
  bool autoAction = false;             // last action was the default one after a timeout
  uint16_t desyncs = 0;
  bool quitPenalty = false;            // we quit in battle
  bool resultRecorded = false;
  PvpResult result = PVP_ABORTED;
  TrainerAward award;
  bool badgeEarned = false;
};
