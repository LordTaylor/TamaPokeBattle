#pragma once

#include <stdint.h>

// Wrap-safe, non-monotonic-tolerant time check.
// Contract: nowMs is a free-running uint32 millisecond counter (millis()). Values
// may wrap around, and a later call may pass a time slightly EARLIER than a
// previous one (e.g. a packet timestamp taken in a callback before loop's
// millis()); an earlier time counts as "not elapsed", never as a timeout.
// `spanMs` must be below 2^31.
inline bool pvpElapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t spanMs) {
  return (int32_t)(nowMs - sinceMs) >= (int32_t)spanMs;
}
