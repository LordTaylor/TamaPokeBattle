#pragma once

#include <stdint.h>

#include "pvp_config.h"

// Table of unacknowledged reliable messages with timed retransmission.
// PVP_RETX_FAST_COUNT retransmits every PVP_RETX_INTERVAL_MS, then every
// PVP_RETX_SLOW_MS until acknowledged or the session ends.
class PvpReliableTx {
 public:
  typedef void (*SendFn)(void *ctx, const uint8_t *data, uint8_t len);

  void reset();
  bool empty() const;
  // Registers a message whose first copy the caller already sent at nowMs.
  bool add(uint8_t type, uint16_t seq, uint16_t round, const uint8_t *data, uint8_t len,
           uint32_t nowMs);
  void ack(uint16_t seq);
  // Removes pending messages of `type` (any round, or only `round` when matchRound).
  void drop(uint8_t type, bool matchRound, uint16_t round);
  void tick(uint32_t nowMs, SendFn send, void *ctx);

 private:
  struct Pending {
    bool active = false;
    uint8_t type = 0;
    uint16_t seq = 0;
    uint16_t round = 0;
    uint8_t len = 0;
    uint8_t sends = 0;
    uint32_t nextMs = 0;
    uint8_t data[PVP_MAX_PACKET] = {0};
  };
  Pending items_[PVP_MAX_PENDING];
};
