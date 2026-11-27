#pragma once

#include <stdint.h>

#include "pvp_event.h"

// Receiver of session events (implemented by PvpApp).
class PvpEventSink {
 public:
  virtual ~PvpEventSink() {}
  virtual void onEvent(const PvpEvent &event, uint32_t nowMs) = 0;
};
