#include "pvp_host_pet.h"

#include <Arduino.h>

#include "battle.h"
#include "dex.h"
#include "pet.h"
#include "pvp_validator.h"
#include "rtcbat.h"

extern Pet pet;  // defined in TamaPoke.ino

namespace {
// Below this charge (and without USB) a fight is refused: the radio drains the battery.
constexpr int PVP_MIN_BATTERY_PCT = 20;
}  // namespace

bool PvpHostPet::localReady(PvpReadyPayload &out) {
  if (pet.isEgg() || pet.speciesId < PVP_DEX_MIN || pet.speciesId > DEX_COUNT) return false;
  BattleStats stats = {};
  stats.level = pet.level();
  stats.atk = pet.atkStat();
  stats.def = pet.defStat();
  stats.spe = pet.speStat();
  stats.hp = 0;
  stats.type1 = DEX_TBL[pet.speciesId].type1;
  stats.type2 = DEX_TBL[pet.speciesId].type2;
  out = pvpReadyFromStats(pet.speciesId, pet.shiny, stats);
  return true;
}

const char *PvpHostPet::nick() const { return pet.trainer; }

uint8_t PvpHostPet::trainerLevel() const { return pet.trainerStats.level; }

bool PvpHostPet::canFight(PvpDenyReason &reason) const {
  reason = PVP_DENY_NONE;
  if (pet.isEgg()) reason = PVP_DENY_EGG;
  else if (pet.sleeping) reason = PVP_DENY_SLEEPING;
  else if (pet.ceremony) reason = PVP_DENY_CEREMONY;
  else if (!pet.hasTrainer()) reason = PVP_DENY_NO_NICK;
  else {
    int pct = batPercent();
    if (pct >= 0 && pct < PVP_MIN_BATTERY_PCT && !usbPresent()) reason = PVP_DENY_LOW_BATTERY;
  }
  return reason == PVP_DENY_NONE;
}

uint32_t PvpHostPet::nowEpoch() const { return rtcEpoch(); }

uint32_t PvpHostPet::random32() { return esp_random(); }

TrainerAward PvpHostPet::recordMatch(PvpResult result, const char *oppNick, size_t oppNickLen,
                                     uint8_t oppLevel, uint32_t oppDeviceId, uint32_t nowEpoch,
                                     bool badgeEligible) {
  return pet.recordPvpMatch(result, oppNick, oppNickLen, oppLevel, oppDeviceId, nowEpoch,
                            badgeEligible);
}

void PvpHostPet::battleBegin() { pet.pvpBattleBegin(); }

void PvpHostPet::battleEnd() { pet.pvpBattleEnd(); }

bool PvpHostPet::recordQuit() { return pet.recordPvpQuit(); }
