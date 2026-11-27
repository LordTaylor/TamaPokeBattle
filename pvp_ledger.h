#pragma once

#include <stddef.h>
#include <stdint.h>

#include "trainer.h"

// Persistent side effects of a PvP match (write side). Implemented on top of Pet.
// Per battle exactly ONE of recordMatch / recordQuit / battleEnd follows battleBegin.
class PvpLedger {
 public:
  virtual ~PvpLedger() {}
  virtual TrainerAward recordMatch(PvpResult result, const char *oppNick, size_t oppNickLen,
                                   uint8_t oppLevel, uint32_t oppDeviceId, uint32_t nowEpoch,
                                   bool badgeEligible) = 0;
  virtual void battleBegin() = 0;
  virtual void battleEnd() = 0;
  virtual bool recordQuit() = 0;
};
