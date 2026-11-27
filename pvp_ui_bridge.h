#pragma once

// What the PvP renderers use from TamaPoke.ino (firmware only). Every symbol below has external
// linkage in the sketch today (spec A.5); nothing here may be made `static` there.
// If the sketch build refuses to link them, fall back to a context struct passed from setup().

#include <Arduino.h>
#include "Arduino_GFX_Library.h"

#include "audio.h"  // sfxPlay, SFX_*
#include "i18n.h"   // T(), S_*, gLang
#include "pet.h"    // Pet, trainer stats, history, badges
#include "pvp_strings.h"  // TP(), PVP_S_*

extern Arduino_Canvas *gfx;        // framebuffer; drawing ends with flushStampedFrame()
extern Pet pet;                    // pet.trainer, pet.trainerStats, pet.pvpHistory, pet.pvpBadges
extern bool cardDirty;             // request a redraw of the card
extern bool cardOpen;
extern uint8_t cardPage;
extern bool screenOff;             // wake: screenOff = false + lastInteract = now
extern uint32_t lastInteract;
extern uint16_t stepsToEncounter;  // 0 = not drawn yet
extern bool stepEncounterPending;

void lockTouchBrief(uint16_t ms);  // defined with a default argument in the sketch; always pass ms
void flushStampedFrame();          // watermark + gfx->flush(); the last call of every frame
