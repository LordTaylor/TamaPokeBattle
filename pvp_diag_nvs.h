#pragma once

#ifdef TAMAPOKE_LOCAL_TEST

#include <stdint.h>

struct PvpDiagNvsStat {
  bool ok = false;
  uint32_t used = 0;
  uint32_t free = 0;
  uint32_t available = 0;
  uint32_t total = 0;
  uint32_t erased = 0;       // derived: total - used - free (nvs_stats_t has no erased field)
  uint32_t namespaces = 0;
  uint32_t freePct = 0;      // free entries, percent of total (includes the reserved page: optimistic)
  uint32_t availPct = 0;     // available / (used + available): excludes the reserved page, the gate value
};

// Reads the default NVS partition and prints "NVSSTAT <tag> ..." plus one line per
// namespace (keys per namespace, to spot nvs.net80211 / phy).
PvpDiagNvsStat pvpDiagNvsPrint(const char *tag);

#endif
