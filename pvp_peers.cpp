#include "pvp_peers.h"

#include <string.h>

const PvpPeerInfo *PvpPeerTable::find(uint32_t deviceId) const {
  for (uint8_t i = 0; i < count_; i++) {
    if (peers_[i].deviceId == deviceId) return &peers_[i];
  }
  return nullptr;
}

PvpPeerTable::UpdateResult PvpPeerTable::update(const PvpMessage &hello, const uint8_t mac[6],
                                                uint32_t nowMs) {
  PvpPeerInfo *slot = nullptr;
  UpdateResult result = UPDATED;
  for (uint8_t i = 0; i < count_; i++) {
    if (peers_[i].deviceId == hello.deviceId) slot = &peers_[i];
  }
  if (!slot) {
    if (count_ >= PVP_MAX_PEERS) return FULL;
    slot = &peers_[count_++];
    result = ADDED;
  }
  slot->deviceId = hello.deviceId;
  memcpy(slot->mac, mac, 6);
  memcpy(slot->nick, hello.nick, PVP_NICK_BUF);
  slot->trainerLevel = hello.trainerLevel;
  slot->version = hello.version;
  slot->busy = hello.busy;
  slot->lastSeenMs = nowMs;
  return result;
}

bool PvpPeerTable::popExpired(uint32_t nowMs, PvpPeerInfo &out) {
  for (uint8_t i = 0; i < count_; i++) {
    if ((uint32_t)(nowMs - peers_[i].lastSeenMs) < PVP_PEER_EXPIRE_MS) continue;
    out = peers_[i];
    peers_[i] = peers_[count_ - 1];
    count_--;
    return true;
  }
  return false;
}
