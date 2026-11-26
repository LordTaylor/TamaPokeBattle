#pragma once

#include <stdint.h>

#include "pvp_config.h"

// Sliding window of received sequence numbers (receiver-side deduplication).
// Bit i of mask_ means "maxSeq_ - i was seen". Sequence 0 is never used.
class PvpSeqWindow {
 public:
  void reset() { maxSeq_ = 0; mask_ = 0; }

  bool isDuplicate(uint16_t seq) const {
    int16_t diff = (int16_t)(seq - maxSeq_);
    if (diff > 0) return false;
    uint16_t back = (uint16_t)(-diff);
    if (back >= PVP_SEQ_WINDOW) return true;  // too old to tell: treat as seen
    return (mask_ >> back) & 1u;
  }

  void mark(uint16_t seq) {
    int16_t diff = (int16_t)(seq - maxSeq_);
    if (diff > 0) {
      mask_ = diff >= PVP_SEQ_WINDOW ? 0u : (mask_ << diff);
      mask_ |= 1u;
      maxSeq_ = seq;
      return;
    }
    uint16_t back = (uint16_t)(-diff);
    if (back < PVP_SEQ_WINDOW) mask_ |= (1u << back);
  }

 private:
  uint16_t maxSeq_ = 0;
  uint32_t mask_ = 0;
};
