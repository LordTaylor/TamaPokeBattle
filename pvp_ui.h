#pragma once

#include <stdint.h>

#include "pvp_ui_theme.h"

// Hooks used by TamaPoke.ino (spec A.4). The sketch includes only this header and knows none of
// the PvP classes.

class PvpApp;
class PvpProfile;

// --- Defined by pvp_app_instance.cpp (they need the global PvpApp) ---
PvpApp &pvpAppInstance();
const PvpProfile &pvpProfileInstance();  // card page: canFight(reason) without opening the radio
void pvpAppUpdate(uint32_t nowMs);       // PvpApp::update(), then pvpUiTrack(nowMs); call every loop (<= ~100 ms)
bool pvpAppActive();                     // model().phase != IDLE
bool pvpAppKeepAwake();                  // PvpApp::keepAwake()
bool pvpAppRadioOn();                    // PvpApp::radioOn()
void pvpAppForceStop(uint32_t nowMs);    // PvpApp::forceStop()

// --- Defined by the pvp_ui_*.cpp renderers ---
inline uint32_t pvpAppRenderIntervalMs() { return pvpui::RENDER_INTERVAL_MS; }
void pvpAppSwipeV(int dir);              // dir < 0 = up (same convention as onSwipeV)
void pvpAppSwipe(int dir);               // dir > 0 = right (same convention as onSwipe)
void pvpAppTap(int16_t x, int16_t y);
void pvpCardResetSub();                  // card sub-view back to the main page (call on every page change)
bool pvpCardTap(int16_t x, int16_t y);   // card page 10; false = not handled (falls to the generic "back")
void renderPvpApp();                     // full-screen PvP frame, ends with flushStampedFrame()
void renderCardPvp();                    // content of card page 10 (the caller adds dots and "back")
void pvpUiTrack(uint32_t nowMs);         // phase-change effects: sounds, wake, toasts, redraw requests
