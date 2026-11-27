#include "pvp_ui_toast.h"

#include "pvp_time.h"

namespace {

bool live = false;
PvpStrId textId = PVP_S_SOON;
uint16_t textColor = 0;
uint32_t sinceMs = 0;
uint32_t spanMs = 0;

}  // namespace

void pvpToastShow(PvpStrId id, uint16_t color, uint32_t nowMs, uint32_t durationMs) {
  live = true;
  textId = id;
  textColor = color;
  sinceMs = nowMs;
  spanMs = durationMs;
}

const char *pvpToastText(uint32_t nowMs, uint16_t &color) {
  if (!live || pvpElapsed(nowMs, sinceMs, spanMs)) return nullptr;
  color = textColor;
  return TP(textId);
}

bool pvpToastJustExpired(uint32_t nowMs) {
  if (!live || !pvpElapsed(nowMs, sinceMs, spanMs)) return false;
  live = false;
  return true;
}
