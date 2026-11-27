#pragma once

#include "pvp_battle.h"
#include "pvp_config.h"

// Tripwire: PVP_RULES_VERSION is only mixed into the state hash, the handshake carries
// PVP_PROTO_VERSION. Different rule versions would show up as a DESYNC in round 1 instead of
// "update the game", so bump both together and update this assert.
static_assert(PVP_PROTO_VERSION == 2 && PVP_RULES_VERSION == 3,
              "bump PVP_PROTO_VERSION together with PVP_RULES_VERSION and update this assert");
