#pragma once

#include <stdint.h>

#include "i18n.h"

// PvP strings live in their own X-macro table (pvp_strings.def), not in the uint8_t StrId.
enum PvpStrId : uint8_t {
#define X(id, maxlen, es, en, fr, de, it, pt, pl) PVP_S_##id,
#include "pvp_strings.def"
#undef X
  PVP_STR_COUNT
};

const char *TP(PvpStrId id);                   // text in the active language; "" outside the range
const char *pvpStrIn(PvpStrId id, Lang lang);  // explicit language (tests); "" outside the range
uint8_t pvpStrMaxLen(PvpStrId id);             // limit from the .def; 0 outside the range
