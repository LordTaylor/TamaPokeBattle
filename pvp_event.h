#pragma once

#include <stdint.h>

#include "pvp_packet.h"
#include "pvp_peer_info.h"

enum PvpEventType : uint8_t {
  PVP_EVT_PEER_FOUND = 1,
  PVP_EVT_PEER_LOST,
  PVP_EVT_CHALLENGE_RECEIVED,  // peer filled
  PVP_EVT_ACCEPTED,            // session established (both sides), pick the Pokemon now
  PVP_EVT_DECLINED,
  PVP_EVT_OPPONENT_READY,      // ready filled
  PVP_EVT_ROUND_READY,         // round, actionA, actionB, luck: run the engine, then submitRoundHash
  PVP_EVT_ROUND_CONFIRMED,     // round: both hashes matched
  PVP_EVT_DESYNC,              // hash mismatch, followed by ABORTED(DESYNC)
  PVP_EVT_ACTION_TIMEOUT,      // round: local action overdue, call submitDefaultAction
  PVP_EVT_ABORTED,             // reason; no points, no badge
  PVP_EVT_FINISHED,            // regular end, result counts
  PVP_EVT_VERSION_MISMATCH,    // peer.mac filled (may be followed by ABORTED(VERSION))
  PVP_EVT_BATTLE_START,        // both READY exchanged: submitAction for round 0
};

struct PvpEvent {
  uint8_t type = 0;
  uint8_t reason = 0;
  uint16_t round = 0;
  uint8_t actionA = 0;
  uint8_t actionB = 0;
  uint8_t luck = 0;
  PvpPeerInfo peer;
  PvpReadyPayload ready;
};
