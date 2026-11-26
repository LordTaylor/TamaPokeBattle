#pragma once

#include <stdint.h>

#include "pvp_config.h"

struct PvpPeerInfo {
  uint32_t deviceId = 0;
  uint8_t mac[6] = {0};
  char nick[PVP_NICK_BUF] = {0};
  uint8_t trainerLevel = 0;
  uint8_t version = 0;
  uint8_t busy = 0;
  uint32_t lastSeenMs = 0;
};
