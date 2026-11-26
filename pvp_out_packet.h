#pragma once

#include <stdint.h>

#include "pvp_config.h"

// A packet waiting to be handed to the transport (ESP-NOW).
struct PvpOutPacket {
  bool broadcast = false;
  uint8_t mac[6] = {0};   // valid when !broadcast
  uint8_t len = 0;
  uint8_t data[PVP_MAX_PACKET] = {0};
};
