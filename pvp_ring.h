#pragma once

#include <stdint.h>

// Fixed-capacity FIFO. push() fails (and counts a drop) when full.
template <typename T, uint8_t N>
class PvpRing {
 public:
  void clear() { head_ = 0; count_ = 0; }
  uint8_t size() const { return count_; }
  uint16_t dropped() const { return dropped_; }

  bool push(const T &item) {
    if (count_ >= N) {
      dropped_++;
      return false;
    }
    items_[(uint8_t)((head_ + count_) % N)] = item;
    count_++;
    return true;
  }

  bool pop(T &out) {
    if (count_ == 0) return false;
    out = items_[head_];
    head_ = (uint8_t)((head_ + 1) % N);
    count_--;
    return true;
  }

 private:
  T items_[N];
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  uint16_t dropped_ = 0;
};
