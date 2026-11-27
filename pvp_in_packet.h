#pragma once

#include <stdint.h>
#include <string.h>

#include "pvp_config.h"

// A received ESP-NOW frame copied out of the radio callback. The callback must do
// nothing but pvpInPacketFrom() + queue send; all protocol logic runs in loop().
struct PvpInPacket {
  uint8_t mac[6] = {0};
  uint8_t len = 0;
  uint8_t data[PVP_MAX_PACKET] = {0};
};

// Boundary filter for the radio callback: rejects null pointers, empty or oversized
// frames and frames without our magic (little-endian on the wire), so foreign ESP-NOW
// traffic never occupies the queue. Returns true and fills `out` only for a frame
// worth queueing; full validation stays in pvpParse().
inline bool pvpInPacketFrom(const uint8_t *src, const uint8_t *data, int len, PvpInPacket &out) {
  if (!src || !data) return false;
  if (len < 2 || len > PVP_MAX_PACKET) return false;
  if (data[0] != (uint8_t)(PVP_MAGIC & 0xFF) || data[1] != (uint8_t)(PVP_MAGIC >> 8)) return false;
  memcpy(out.mac, src, 6);
  out.len = (uint8_t)len;
  memcpy(out.data, data, (size_t)len);
  return true;
}
