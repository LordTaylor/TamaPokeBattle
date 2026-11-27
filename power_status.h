#pragma once
#include <stdint.h>

// Stan zasilania pokazywany w nakladce na kazdym ekranie.
enum PowerState : uint8_t {
  POWER_NO_BATTERY = 0,  // brak akumulatora: nakladka ukryta
  POWER_CHARGING,        // ladowanie w toku
  POWER_USB,             // zasilanie z USB, ladowanie nieaktywne (np. pelny)
  POWER_BATTERY          // praca z akumulatora
};

inline PowerState powerStateOf(int percent, bool charging, bool usbPresent) {
  if (percent < 0) return POWER_NO_BATTERY;
  if (charging) return POWER_CHARGING;
  return usbPresent ? POWER_USB : POWER_BATTERY;
}

// Klucz do wykrywania zmiany tego, co nakladka pokazuje (stan + procent).
inline int32_t powerChipKey(int percent, PowerState state) {
  int pc = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
  return ((int32_t)state << 8) | pc;
}

// Procent z napiecia akumulatora LiPo (mV), gdy gauge AXP2101 nie ma profilu
// akumulatora i zwraca 0. Kolejne punkty krzywej: {mV, %}; miedzy nimi interpolacja.
struct LipoPoint { uint16_t mv; uint8_t pct; };
static const LipoPoint LIPO_CURVE[] = {
  {3300, 0}, {3500, 5}, {3600, 15}, {3700, 35}, {3800, 55},
  {3900, 70}, {4000, 82}, {4100, 92}, {4200, 100},
};
static const int LIPO_GAUGE_INVALID_MIN_MV = 3400;  // gauge=0 przy wyzszym napieciu = brak profilu

inline int lipoPercentFromMv(int mv) {
  const int n = (int)(sizeof(LIPO_CURVE) / sizeof(LIPO_CURVE[0]));
  if (mv <= LIPO_CURVE[0].mv) return 0;
  if (mv >= LIPO_CURVE[n - 1].mv) return 100;
  for (int i = 1; i < n; i++) {
    if (mv <= LIPO_CURVE[i].mv) {
      int dmv = LIPO_CURVE[i].mv - LIPO_CURVE[i - 1].mv;
      int dpc = LIPO_CURVE[i].pct - LIPO_CURVE[i - 1].pct;
      return LIPO_CURVE[i - 1].pct + (mv - LIPO_CURVE[i - 1].mv) * dpc / dmv;
    }
  }
  return 100;
}

// Wybor zrodla procentu: gauge, a gdy jest ewidentnie niewazny (0% przy napieciu
// swiadczacym o naladowanym akumulatorze) - krzywa napieciowa.
inline int resolveBatteryPercent(int gaugePct, int mv) {
  if (gaugePct < 0) return -1;  // brak akumulatora
  if (gaugePct == 0 && mv >= LIPO_GAUGE_INVALID_MIN_MV) return lipoPercentFromMv(mv);
  return gaugePct;
}
