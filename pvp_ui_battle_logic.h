#pragma once

#include <stdint.h>

#include "pvp_battle.h"
#include "pvp_strings.h"
#include "trainer.h"

// Pure helpers of the PvP battle / result screens (no Arduino, host-testable): which text a round
// produces, countdown arithmetic, low-HP rule, reason for a blocked award, paging.

enum PvpTone : uint8_t { PVP_TONE_INK = 0, PVP_TONE_WARN, PVP_TONE_BAD };

struct PvpLogLine {
  char text[48];  // empty = nothing to show
  PvpTone tone;
};

// Line 1: what my action did (spec C.5, first table). Line 2: what the rival did to me.
void pvpMineLine(const PvpTurnView &view, PvpLogLine &out);
void pvpFoeLine(const PvpTurnView &view, PvpLogLine &out);

// Round clock shown on screen (decisions are made by the session, never by this value).
// Wrap-safe; a time before `startMs` counts as "nothing spent yet".
int pvpRoundLeftPermille(uint32_t nowMs, uint32_t startMs, uint32_t totalMs);  // 0..1000
int pvpRoundSecondsLeft(uint32_t nowMs, uint32_t startMs, uint32_t totalMs);   // rounded up, 0 at the end

// > 50 % left = INK (caller draws "ok"), 25..50 % = WARN, below = BAD.
PvpTone pvpTimeTone(int leftPermille);

bool pvpHpLow(uint16_t hp, uint16_t maxHp);  // alive and at or below 30 % of max

// Why a settled match gave no points: no clock vs. the same rival met within the cooldown.
PvpStrId pvpBlockedText(uint32_t nowEpoch);

uint8_t pvpPageCount(uint8_t items, uint8_t perPage);  // at least 1
