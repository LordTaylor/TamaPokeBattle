#include "pvp_pump.h"

void PvpPump::sendAll() {
  PvpOutPacket p;
  while (session_.popPacket(p)) link_.send(p);  // a failed send is retransmitted by the session
}

void PvpPump::drain(uint32_t nowMs, PvpEventSink &sink) {
  PvpEvent e;
  for (uint8_t i = 0; i < PVP_PUMP_MAX_EVENTS; i++) {
    sendAll();
    if (!session_.popEvent(e)) return;
    sink.onEvent(e, nowMs);
  }
  sendAll();
}

void PvpPump::update(uint32_t nowMs, PvpEventSink &sink) {
  PvpInPacket in;
  for (uint8_t i = 0; i < PVP_PUMP_MAX_RX; i++) {
    if (!link_.receive(in)) break;
    session_.onPacket(in.data, in.len, in.mac, nowMs);
    drain(nowMs, sink);
  }
  session_.tick(nowMs);
  drain(nowMs, sink);
}
