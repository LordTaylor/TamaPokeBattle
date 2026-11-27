#pragma once

#include <stdint.h>

#include "pvp_deny_reason.h"
#include "pvp_packet.h"

// What the PvP layer needs to know about the player (read side). Implemented by
// the firmware on top of Pet, by a fake in tests.
class PvpProfile {
 public:
  virtual ~PvpProfile() {}
  // READY payload of the active Pokemon. hp is forced to 0 by the caller.
  virtual bool localReady(PvpReadyPayload &out) = 0;
  virtual const char *nick() const = 0;
  virtual uint8_t trainerLevel() const = 0;
  virtual bool canFight(PvpDenyReason &reason) const = 0;
  virtual uint32_t nowEpoch() const = 0;  // 0 = no valid clock
  virtual uint32_t random32() = 0;
};
