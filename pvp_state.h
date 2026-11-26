#pragma once

#include <stdint.h>

enum PvpState : uint8_t {
  PVP_STATE_IDLE = 0,
  PVP_STATE_DISCOVERY,
  PVP_STATE_CHALLENGING,   // our CHALLENGE sent, waiting for ACCEPT/DECLINE
  PVP_STATE_CHALLENGED,    // peer's CHALLENGE received, waiting for accept()/decline()
  PVP_STATE_HANDSHAKE,     // session and nonces established, waiting for setLocalReady()
  PVP_STATE_READY_EXCHANGE,  // local READY sent, waiting for the peer's READY
  PVP_STATE_BATTLE,
  PVP_STATE_FINISHED,      // regular end; lingers to deliver the last retransmits
  PVP_STATE_ABORTED,       // no points, no badge
};

enum PvpRoundPhase : uint8_t {
  PVP_PHASE_WAIT_ACTIONS = 0,
  PVP_PHASE_WAIT_HASH,
};
