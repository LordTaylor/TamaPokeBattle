#pragma once

#include <stdint.h>

#include "pvp_event.h"
#include "pvp_event_sink.h"
#include "pvp_link.h"
#include "pvp_session.h"

// Moves data between PvpLink and PvpSession. Order guarantees:
//  * received packets are handled before the tick;
//  * every queued packet is sent BEFORE the next event reaches the sink, so a
//    handler that blocks (NVS save) never delays the packets produced before it.
class PvpPump {
 public:
  PvpPump(PvpLink &link, PvpSession &session) : link_(link), session_(session) {}
  // At most PVP_PUMP_MAX_RX packets, then tick, draining after each step.
  void update(uint32_t nowMs, PvpEventSink &sink);
  // Sends all queued packets and delivers all events until both queues are empty
  // (bounded). Call after every caller action on the session.
  void drain(uint32_t nowMs, PvpEventSink &sink);

 private:
  void sendAll();

  PvpLink &link_;
  PvpSession &session_;
};
