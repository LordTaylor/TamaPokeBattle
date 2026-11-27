#pragma once

#include "pvp_ledger.h"
#include "pvp_profile.h"

// Firmware adapter: PvP profile (read) and ledger (write) on top of the global Pet.
class PvpHostPet : public PvpProfile, public PvpLedger {
 public:
  // PvpProfile
  bool localReady(PvpReadyPayload &out) override;
  const char *nick() const override;
  uint8_t trainerLevel() const override;
  bool canFight(PvpDenyReason &reason) const override;
  uint32_t nowEpoch() const override;
  uint32_t random32() override;
  // PvpLedger
  TrainerAward recordMatch(PvpResult result, const char *oppNick, size_t oppNickLen,
                           uint8_t oppLevel, uint32_t oppDeviceId, uint32_t nowEpoch,
                           bool badgeEligible) override;
  void battleBegin() override;
  void battleEnd() override;
  bool recordQuit() override;
};
