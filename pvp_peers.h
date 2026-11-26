#pragma once

#include <stdint.h>

#include "pvp_packet.h"
#include "pvp_peer_info.h"

// Discovered peers (max PVP_MAX_PEERS), expiring after PVP_PEER_EXPIRE_MS without HELLO.
class PvpPeerTable {
 public:
  enum UpdateResult : uint8_t { ADDED, UPDATED, FULL, REJECTED };  // REJECTED: id seen with another MAC

  void clear() { count_ = 0; }
  uint8_t count() const { return count_; }
  const PvpPeerInfo &at(uint8_t i) const { return peers_[i]; }
  const PvpPeerInfo *find(uint32_t deviceId) const;

  UpdateResult update(const PvpMessage &hello, const uint8_t mac[6], uint32_t nowMs);
  // Removes one expired peer into `out`; returns false when none expired.
  bool popExpired(uint32_t nowMs, PvpPeerInfo &out);

 private:
  PvpPeerInfo peers_[PVP_MAX_PEERS];
  uint8_t count_ = 0;
};
