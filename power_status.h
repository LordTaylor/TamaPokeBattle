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
