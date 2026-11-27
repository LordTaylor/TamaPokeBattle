#pragma once

#include <stdint.h>

// millis() at which the current PvP phase began (set by pvpUiTrack); drives the countdown bars.
uint32_t pvpUiPhaseSinceMs();
