#ifdef TAMAPOKE_LOCAL_TEST

#include "pvp_diag_nvs.h"

#include <Arduino.h>
#include <nvs.h>
#include <string.h>

#include "pvp_diag_config.h"

PvpDiagNvsStat pvpDiagNvsPrint(const char *tag) {
  PvpDiagNvsStat st;
  nvs_stats_t s;
  if (nvs_get_stats(nullptr, &s) != ESP_OK) {
    Serial.printf("NVSSTAT %s ERR nvs_get_stats\n", tag);
    return st;
  }
  st.ok = true;
  st.used = s.used_entries;
  st.free = s.free_entries;
  st.available = s.available_entries;
  st.total = s.total_entries;
  st.erased = s.total_entries - s.used_entries - s.free_entries;
  st.namespaces = s.namespace_count;
  st.freePct = s.total_entries ? (uint32_t)((100ULL * s.free_entries) / s.total_entries) : 0;
  uint32_t usable = s.used_entries + s.available_entries;
  st.availPct = usable ? (uint32_t)((100ULL * s.available_entries) / usable) : 0;
  Serial.printf("NVSSTAT %s used=%u free=%u avail=%u total=%u erased(derived)=%u ns=%u free%%=%u avail%%=%u\n", tag,
                (unsigned)st.used, (unsigned)st.free, (unsigned)st.available, (unsigned)st.total,
                (unsigned)st.erased, (unsigned)st.namespaces, (unsigned)st.freePct, (unsigned)st.availPct);

  char names[PVP_DIAG_NVS_MAX_NAMESPACES][NVS_NS_NAME_MAX_SIZE];
  uint16_t counts[PVP_DIAG_NVS_MAX_NAMESPACES] = {0};
  uint8_t nNames = 0;
  uint32_t keys = 0;
  nvs_iterator_t it = nullptr;
  esp_err_t err = nvs_entry_find(PVP_DIAG_NVS_PARTITION, nullptr, NVS_TYPE_ANY, &it);
  while (err == ESP_OK) {
    nvs_entry_info_t info;
    if (nvs_entry_info(it, &info) == ESP_OK) {
      keys++;
      uint8_t i = 0;
      while (i < nNames && strncmp(names[i], info.namespace_name, sizeof(names[i])) != 0) i++;
      if (i == nNames && nNames < PVP_DIAG_NVS_MAX_NAMESPACES) {
        strncpy(names[nNames], info.namespace_name, sizeof(names[nNames]) - 1);
        names[nNames][sizeof(names[nNames]) - 1] = 0;
        nNames++;
      }
      if (i < nNames) counts[i]++;
    }
    err = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  for (uint8_t i = 0; i < nNames; i++)
    Serial.printf("NVSSTAT %s ns '%s' keys=%u\n", tag, names[i], (unsigned)counts[i]);
  Serial.printf("NVSSTAT %s keys_total=%u (blob keys span several entries)\n", tag, (unsigned)keys);
  return st;
}

#endif
