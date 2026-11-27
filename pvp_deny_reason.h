#pragma once

#include <stdint.h>

enum PvpDenyReason : uint8_t {
  PVP_DENY_NONE = 0,
  PVP_DENY_EGG,
  PVP_DENY_SLEEPING,
  PVP_DENY_CEREMONY,
  PVP_DENY_NO_NICK,
  PVP_DENY_LOW_BATTERY,
};
