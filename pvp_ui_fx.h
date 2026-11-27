#pragma once

#include <stdint.h>

#include "pvp_app.h"

// Battle-side effects of the PvP UI (firmware only): rival sprite, sounds, delayed jingles
// (spec C.5, C.7, C.12). Called from pvpUiTrack on phase changes and every loop.

void pvpFxBattleStart(const PvpUiModel &model, uint32_t nowMs);  // sprite load, start jingle, chirp
void pvpFxRoundResolved(const PvpApp &app);                      // one sound for the round just played
void pvpFxResult(const PvpUiModel &model, uint32_t nowMs);       // win / loss jingle, level, badge, blocked
void pvpFxBattleEnd();                                           // rival sprite out of memory
void pvpFxTick(const PvpApp &app, uint32_t nowMs);               // 5 s warning, delayed jingles
