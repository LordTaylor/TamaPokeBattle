#include "pvp_ui_battle_logic.h"

#include <stdio.h>
#include <string.h>

#include "battle_core.h"
#include "i18n.h"

namespace {

void clear(PvpLogLine &out) {
  out.text[0] = '\0';
  out.tone = PVP_TONE_INK;
}

void setText(PvpLogLine &out, PvpTone tone, const char *text) {
  snprintf(out.text, sizeof(out.text), "%s", text);
  out.tone = tone;
}

void appendNote(PvpLogLine &out, const char *note) {
  size_t used = strlen(out.text);
  snprintf(out.text + used, sizeof(out.text) - used, "%s%s", used ? " " : "", note);
}

void damageText(const PvpSideTurn &m, PvpLogLine &out) {
  char buf[48];
  if (m.typePct > 100) snprintf(buf, sizeof(buf), "%s %u", T(S_EFFECTIVE), (unsigned)m.damageDealt);
  else if (m.typePct < 100) snprintf(buf, sizeof(buf), "%s %u", T(S_NOT_EFFECTIVE), (unsigned)m.damageDealt);
  else snprintf(buf, sizeof(buf), T(S_HIT_FMT), (unsigned)m.damageDealt);
  setText(out, PVP_TONE_INK, buf);
}

}  // namespace

void pvpMineLine(const PvpTurnView &v, PvpLogLine &out) {
  clear(out);
  const PvpSideTurn &m = v.mine;
  char buf[48];
  if (m.restFailed) {
    setText(out, PVP_TONE_BAD, T(S_NO_REST));
  } else if (m.rested) {
    snprintf(buf, sizeof(buf), T(S_RESTED_FMT), (unsigned)m.healed);
    setText(out, PVP_TONE_INK, buf);
    appendNote(out, T(S_GUARD));
  } else if (m.damageDealt > 0) {
    damageText(m, out);
  } else if (battle_core::isAttackAction(m.action)) {
    bool evaded = v.theirs.evaded || v.theirs.dodged;
    setText(out, PVP_TONE_INK, evaded ? T(S_ENEMY_DODGED) : T(S_MISSED));
  } else if (m.action == BATTLE_DODGE && m.dodged) {
    setText(out, PVP_TONE_INK, T(S_DODGED));
  }
  if (m.counterGained || m.counterUsed) {
    appendNote(out, T(S_COUNTER_READY));
    out.tone = PVP_TONE_WARN;
  }
}

void pvpFoeLine(const PvpTurnView &v, PvpLogLine &out) {
  clear(out);
  char buf[48];
  if (v.theirs.damageDealt > 0) {
    snprintf(buf, sizeof(buf), TP(PVP_S_FOE_HIT_FMT), (unsigned)v.theirs.damageDealt);
    setText(out, PVP_TONE_BAD, buf);
  } else if (battle_core::isAttackAction(v.theirs.action)) {
    setText(out, PVP_TONE_INK, TP(PVP_S_FOE_MISS));
  }
}

int pvpRoundLeftPermille(uint32_t nowMs, uint32_t startMs, uint32_t totalMs) {
  if (totalMs == 0) return 0;
  int32_t spent = (int32_t)(nowMs - startMs);
  if (spent < 0) spent = 0;  // a time before the start counts as nothing spent
  if ((uint32_t)spent >= totalMs) return 0;
  return (int)((totalMs - (uint32_t)spent) * 1000 / totalMs);
}

int pvpRoundSecondsLeft(uint32_t nowMs, uint32_t startMs, uint32_t totalMs) {
  int32_t spent = (int32_t)(nowMs - startMs);
  if (spent < 0) spent = 0;
  if ((uint32_t)spent >= totalMs) return 0;
  return (int)((totalMs - (uint32_t)spent + 999) / 1000);
}

PvpTone pvpTimeTone(int leftPermille) {
  if (leftPermille > 500) return PVP_TONE_INK;
  if (leftPermille >= 250) return PVP_TONE_WARN;
  return PVP_TONE_BAD;
}

bool pvpHpLow(uint16_t hp, uint16_t maxHp) { return hp > 0 && (uint32_t)hp * 10 <= (uint32_t)maxHp * 3; }

PvpStrId pvpBlockedText(uint32_t nowEpoch) { return nowEpoch == 0 ? PVP_S_BLOCKED_CLOCK : PVP_S_BLOCKED_SAME_RIVAL; }

uint8_t pvpPageCount(uint8_t items, uint8_t perPage) {
  if (perPage == 0 || items == 0) return 1;
  return (uint8_t)((items + perPage - 1) / perPage);
}
