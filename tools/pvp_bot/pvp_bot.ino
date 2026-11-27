// PvP test opponent for a second ESP32-S3 (no display needed). Runs the same PvP core
// as the game (PvpApp over ESP-NOW): accepts challenges, picks actions, reopens after
// every match. Serial (115200): 'c' = challenge the first peer seen, 's' = status.
#include <Arduino.h>

#include "dex.h"
#include "pvp_app.h"
#include "pvp_espnow_link.h"
#include "pvp_validator.h"

namespace {

constexpr int16_t BOT_DEX = 4;                // Charmander
constexpr uint8_t BOT_LEVEL = 12;
constexpr char BOT_NICK[] = "BOTEK";
constexpr uint8_t BOT_TRAINER_LEVEL = 1;
constexpr uint32_t ACTION_DELAY_MS = 900;     // looks like a player, keeps rounds readable
constexpr uint32_t RESULT_HOLD_MS = 4000;
constexpr uint32_t REOPEN_DELAY_MS = 2500;
constexpr uint8_t ATTACK_WEIGHT_PCT = 60;     // otherwise a random action

class BotProfile : public PvpProfile {
 public:
  bool localReady(PvpReadyPayload &out) override {
    const DexEntry &d = DEX_TBL[BOT_DEX];
    BattleStats stats = {};
    stats.level = BOT_LEVEL;
    PvpStatRange atk = pvpStatRange(d.bAtk, BOT_LEVEL);
    PvpStatRange def = pvpStatRange(d.bDef, BOT_LEVEL);
    PvpStatRange spe = pvpStatRange(d.bSpe, BOT_LEVEL);
    stats.atk = (atk.lo + atk.hi) / 2;
    stats.def = (def.lo + def.hi) / 2;
    stats.spe = (spe.lo + spe.hi) / 2;
    stats.type1 = d.type1;
    stats.type2 = d.type2;
    out = pvpReadyFromStats(BOT_DEX, false, stats);
    return true;
  }
  const char *nick() const override { return BOT_NICK; }
  uint8_t trainerLevel() const override { return BOT_TRAINER_LEVEL; }
  bool canFight(PvpDenyReason &reason) const override {
    reason = PVP_DENY_NONE;
    return true;
  }
  uint32_t nowEpoch() const override { return 0; }  // no clock: the farm guard stays off
  uint32_t random32() override { return esp_random(); }
};

class BotLedger : public PvpLedger {
 public:
  TrainerAward recordMatch(PvpResult result, const char *, size_t, uint8_t, uint32_t, uint32_t,
                           bool) override {
    Serial.printf("BOT match result=%d\n", (int)result);
    return TrainerAward();
  }
  void battleBegin() override { Serial.println("BOT battle begin"); }
  void battleEnd() override {}
  bool recordQuit() override { return false; }
};

PvpEspNowLink gLink;
BotProfile gProfile;
BotLedger gLedger;
PvpApp gApp(gLink, gProfile, gLedger);

PvpPhase gLastPhase = PVP_UI_IDLE;
uint32_t gPhaseSince = 0;
uint16_t gActionRound = 0;  // round already answered; a new round restarts the delay
uint32_t gReopenAt = 0;
bool gChallengeRequested = false;

const char *phaseName(PvpPhase p) {
  static const char *const NAMES[] = {"IDLE", "SEARCHING", "CHALLENGING", "CHALLENGED", "PREPARING",
                                      "CHOOSE", "WAIT_OPP", "WAIT_CONFIRM", "WAIT_FINAL", "RESULT",
                                      "ENDED", "STOPPING"};
  return p <= PVP_UI_STOPPING ? NAMES[p] : "?";
}

uint8_t pickAction() {
  if (esp_random() % 100 < ATTACK_WEIGHT_PCT) return PVP_DEFAULT_ACTION;
  return (uint8_t)(esp_random() % PVP_ACTION_COUNT);
}

void drive(uint32_t now) {
  const PvpUiModel &m = gApp.model();
  switch (m.phase) {
    case PVP_UI_IDLE:
      if (now >= gReopenAt) {
        bool ok = gApp.open(now);
        Serial.printf("BOT open=%d\n", ok);
        if (!ok) gReopenAt = now + REOPEN_DELAY_MS;
      }
      break;
    case PVP_UI_SEARCHING:
      gApp.touch(now);
      if (gChallengeRequested && m.peerCount > 0) {
        gChallengeRequested = false;
        Serial.printf("BOT challenge=%d\n", gApp.challengePeer(0, now));
      }
      break;
    case PVP_UI_CHALLENGING:
      gApp.touch(now);
      break;
    case PVP_UI_CHALLENGED:
      gApp.touch(now);
      Serial.printf("BOT accept=%d\n", gApp.respondChallenge(true, now));
      break;
    case PVP_UI_CHOOSE:
      gApp.touch(now);
      if (m.round != gActionRound && now - gPhaseSince >= ACTION_DELAY_MS) {
        gActionRound = m.round;
        uint8_t a = pickAction();
        Serial.printf("BOT round=%u action=%u ok=%d\n", m.round, a, gApp.submitAction(a, now));
      }
      break;
    case PVP_UI_RESULT:
    case PVP_UI_ENDED:
      if (now - gPhaseSince >= RESULT_HOLD_MS) {
        gApp.leave(now);
        gReopenAt = now + REOPEN_DELAY_MS;
      }
      break;
    default:
      gApp.touch(now);
      break;
  }
}

void report(uint32_t now) {
  const PvpUiModel &m = gApp.model();
  if (m.phase != gLastPhase) {
    Serial.printf("BOT phase %s -> %s round=%u end=%d result=%d\n", phaseName(gLastPhase),
                  phaseName(m.phase), m.round, (int)m.endReason, (int)m.result);
    gLastPhase = m.phase;
    gPhaseSince = now;
  }
  static uint16_t lastRound = 0;
  if (m.round != lastRound) {
    lastRound = m.round;
    gPhaseSince = now;
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'c') gChallengeRequested = true;
    if (c == 's') {
      Serial.printf("BOT status phase=%s peers=%u radio=%d heap=%u\n", phaseName(gApp.model().phase),
                    gApp.model().peerCount, gApp.radioOn(), ESP.getFreeHeap());
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("BOT ready (PvP core over ESP-NOW)");
}

void loop() {
  uint32_t now = millis();
  gApp.update(now);
  report(now);
  drive(now);
  handleSerial();
  delay(10);
}
