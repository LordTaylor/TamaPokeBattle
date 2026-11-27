#pragma once

#include <stdint.h>

// Legal band of an atk/def/spe value.
struct PvpStatRange {
  uint16_t lo;
  uint16_t hi;
};
