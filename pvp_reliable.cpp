#include "pvp_reliable.h"

#include <string.h>

void PvpReliableTx::reset() {
  for (Pending &p : items_) p.active = false;
}

bool PvpReliableTx::empty() const {
  for (const Pending &p : items_) {
    if (p.active) return false;
  }
  return true;
}

bool PvpReliableTx::add(uint8_t type, uint16_t seq, uint16_t round, const uint8_t *data,
                        uint8_t len, uint32_t nowMs) {
  if (len > PVP_MAX_PACKET) return false;
  for (Pending &p : items_) {
    if (p.active) continue;
    p.active = true;
    p.type = type;
    p.seq = seq;
    p.round = round;
    p.len = len;
    p.sends = 1;
    p.nextMs = nowMs + PVP_RETX_INTERVAL_MS;
    memcpy(p.data, data, len);
    return true;
  }
  return false;
}

void PvpReliableTx::ack(uint16_t seq) {
  for (Pending &p : items_) {
    if (p.active && p.seq == seq) p.active = false;
  }
}

void PvpReliableTx::drop(uint8_t type, bool matchRound, uint16_t round) {
  for (Pending &p : items_) {
    if (p.active && p.type == type && (!matchRound || p.round == round)) p.active = false;
  }
}

void PvpReliableTx::tick(uint32_t nowMs, SendFn send, void *ctx) {
  for (Pending &p : items_) {
    if (!p.active || (int32_t)(nowMs - p.nextMs) < 0) continue;
    send(ctx, p.data, p.len);
    if (p.sends < 255) p.sends++;
    bool fast = p.sends <= PVP_RETX_FAST_COUNT;  // sends counts the original too
    p.nextMs = nowMs + (fast ? PVP_RETX_INTERVAL_MS : PVP_RETX_SLOW_MS);
  }
}
