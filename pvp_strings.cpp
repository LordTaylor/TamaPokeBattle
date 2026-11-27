#include "pvp_strings.h"

namespace {

constexpr bool hasFormat(const char *s) { return *s ? (*s == '%' || hasFormat(s + 1)) : false; }

static_assert(LANG_COUNT == 7, "pvp_strings.def has 7 language columns (ES EN FR DE IT PT PL)");

// Literals without a conversion are measured by the compiler; formats are measured by i18n_tests.
#define PVP_CHECK(id, maxlen, lang, lit)                                          \
  static_assert(sizeof(lit) > 1, #id " " lang ": empty");                         \
  static_assert(hasFormat(lit) || sizeof(lit) - 1 <= (maxlen), #id " " lang ": too long");
#define X(id, maxlen, es, en, fr, de, it, pt, pl) \
  PVP_CHECK(id, maxlen, "ES", es)                 \
  PVP_CHECK(id, maxlen, "EN", en)                 \
  PVP_CHECK(id, maxlen, "FR", fr)                 \
  PVP_CHECK(id, maxlen, "DE", de)                 \
  PVP_CHECK(id, maxlen, "IT", it)                 \
  PVP_CHECK(id, maxlen, "PT", pt)                 \
  PVP_CHECK(id, maxlen, "PL", pl)
#include "pvp_strings.def"
#undef X
#undef PVP_CHECK

const char *const TABLE[PVP_STR_COUNT][LANG_COUNT] = {
#define X(id, maxlen, es, en, fr, de, it, pt, pl) {es, en, fr, de, it, pt, pl},
#include "pvp_strings.def"
#undef X
};

const uint8_t MAXLEN[PVP_STR_COUNT] = {
#define X(id, maxlen, es, en, fr, de, it, pt, pl) maxlen,
#include "pvp_strings.def"
#undef X
};

}  // namespace

const char *pvpStrIn(PvpStrId id, Lang lang) {
  if ((unsigned)id >= PVP_STR_COUNT) return "";
  if ((unsigned)lang >= LANG_COUNT) lang = LANG_DEFAULT;
  return TABLE[id][lang];
}

const char *TP(PvpStrId id) { return pvpStrIn(id, gLang); }

uint8_t pvpStrMaxLen(PvpStrId id) { return (unsigned)id < PVP_STR_COUNT ? MAXLEN[id] : 0; }
