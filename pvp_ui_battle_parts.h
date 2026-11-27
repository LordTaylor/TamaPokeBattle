#pragma once

#include <stdint.h>

#include "pvp_app.h"

// Pieces of the battle screen (firmware only). pvp_ui_battle_scene.cpp draws the top half (clock,
// names, HP, sprites), pvp_ui_battle.cpp the log and the action buttons, pvp_ui_quit.cpp the modal.

uint16_t pvpBattleInk();  // scene text color (night aware)

void pvpBattleDrawHeader(const PvpApp &app, uint32_t now, uint16_t ink);
void pvpBattleDrawSides(const PvpApp &app, uint32_t now, uint16_t ink);
void pvpBattleDrawSprites(const PvpApp &app, uint32_t now);

// Rival's last-round damage and mine, shown as "-N" next to the HP numbers for a moment.
bool pvpBattleShowDamage(const PvpApp &app, uint32_t now);

// Quit modal (C.6): state lives in pvp_ui_quit.cpp.
bool pvpQuitOpen(const PvpApp &app);  // false (and closed) once the phase is no longer a running battle
void pvpQuitShow();
void pvpQuitClose();  // a new match never inherits an open modal
void pvpQuitDraw();
void pvpQuitTap(PvpApp &app, int16_t x, int16_t y, uint32_t now);
