// Sprite and sound effects of the PvP battle (spec C.5, C.7, C.12). Only existing SFX are used.
#include "pvp_app.h"
#include "pvp_ui_battle_logic.h"
#include "pvp_ui_battle_parts.h"
#include "pvp_ui_bridge.h"
#include "pvp_ui_fx.h"
#include "pvp_ui_phase.h"
#include "pvp_ui_theme.h"
#include "pvp_time.h"
#include "battle_core.h"

namespace {

constexpr uint8_t QUEUE = 2;

struct Delayed {
  uint32_t at;
  uint8_t sfx;
};

Delayed queue[QUEUE];
uint8_t queued = 0;
bool lowHpPlayed = false;
bool warnPlayed = false;

void later(uint8_t sfx, uint32_t nowMs, uint32_t delayMs) {
  if (queued >= QUEUE) return;
  queue[queued].at = nowMs + delayMs;
  queue[queued].sfx = sfx;
  queued++;
}

// The sound of my own action, in the priority order of the PvE battle.
uint8_t roundSfx(const PvpTurnView &v) {
  const PvpSideTurn &m = v.mine;
  if (m.restFailed) return SFX_DENY;
  if (m.counterGained || m.counterUsed) return SFX_COUNTER;
  if (m.rested) return SFX_REST;
  if (m.action == BATTLE_ATTACK_QUICK) return SFX_ATTACK_QUICK;
  if (m.action == BATTLE_ATTACK_HEAVY) return SFX_ATTACK_HEAVY;
  if (m.damageDealt > 0 && m.typePct > 100) return SFX_EFFECTIVE;
  if (m.damageDealt > 0 && m.typePct < 100) return SFX_WEAK_HIT;
  if (v.theirs.damageDealt > 0) return SFX_ENEMY_HIT;
  return m.damageDealt > 0 ? SFX_PLAY : SFX_TAP;
}

}  // namespace

void pvpFxBattleStart(const PvpUiModel &model, uint32_t nowMs) {
  (void)nowMs;
  lowHpPlayed = false;
  warnPlayed = false;
  queued = 0;
  pvpQuitClose();
  wildPmd.unload();
  wildPmd.load(model.oppReady.dex, model.oppReady.shiny != 0);  // missing sprite: the screen falls back to the thumbnail
  sfxPlay(SFX_GAME_START);
  speciesChirpPlay(model.oppReady.dex);
}

void pvpFxRoundResolved(const PvpApp &app) {
  warnPlayed = false;  // a new round clock starts with the confirmation
  const PvpMatch &match = app.match();
  const PvpTurnView &turn = match.lastTurn();
  const PvpSide &mine = pvpMySide(match.runtime(), match.side());
  if (turn.battleEnded) return;  // the result jingle follows with the RESULT screen
  if (!lowHpPlayed && pvpHpLow(mine.hp, mine.maxHp)) {
    lowHpPlayed = true;
    sfxPlay(SFX_LOW_HP);
    return;
  }
  sfxPlay(roundSfx(turn));
}

void pvpFxResult(const PvpUiModel &model, uint32_t nowMs) {
  queued = 0;
  if (model.result == PVP_WIN) sfxPlay(SFX_BATTLE_WIN);
  else if (model.result == PVP_LOSS) sfxPlay(SFX_BATTLE_LOSS);
  else sfxPlay(SFX_MENU);
  if (model.award.levelsGained > 0) later(SFX_LEVEL, nowMs, pvpui::RESULT_BADGE_DELAY_MS);
  if (model.badgeEarned) later(SFX_MEDAL, nowMs, 2 * pvpui::RESULT_BADGE_DELAY_MS);
  if (model.award.blocked) later(SFX_DENY, nowMs, pvpui::RESULT_BADGE_DELAY_MS);
}

void pvpFxBattleEnd() {
  pvpQuitClose();
  wildPmd.unload();
  queued = 0;
}

void pvpFxTick(const PvpApp &app, uint32_t nowMs) {
  const PvpUiModel &m = app.model();
  if (m.phase == PVP_UI_CHOOSE) {
    uint32_t left = PVP_ACTION_TIMEOUT_MS - pvpui::BATTLE_WARN_LEFT_MS;
    if (!warnPlayed && pvpElapsed(nowMs, m.roundStartMs, left)) {
      warnPlayed = true;
      sfxPlay(SFX_MENU);  // one beep, no ticking: audio is a task with delay
    }
  }
  for (uint8_t i = 0; i < queued;) {
    if (!pvpElapsed(nowMs, queue[i].at, 0)) {
      i++;
      continue;
    }
    sfxPlay(queue[i].sfx);
    queue[i] = queue[--queued];
  }
}
