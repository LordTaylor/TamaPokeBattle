#pragma once

#ifdef TAMAPOKE_LOCAL_TEST

#include <stdint.h>

#include "pvp_in_packet.h"

// A received frame plus the RSSI taken from the radio header (diagnostics only; the
// production PvpInPacket carries no RSSI).
struct PvpDiagRx {
  PvpInPacket packet;
  int8_t rssi = 0;
};

struct PvpDiagHeap {
  uint32_t freeHeap = 0;
  uint32_t minFreeHeap = 0;
  uint32_t maxBlock = 0;
};

PvpDiagHeap pvpDiagHeap();

// Reference radio bring-up written exactly as spec B.3/B.6 (T4.2 will turn it into
// PvpEspNowLink). Static functions: there is exactly one radio.
namespace PvpDiagRadio {
bool start(uint8_t outMac[6]);       // persistent(false), setSleep(NONE), mode(STA), channel, ESP-NOW + broadcast peer
void stop();                         // unregister_recv_cb -> esp_now_deinit -> WiFi OFF (queue stays)
bool running();
bool everStarted();                  // true once start() ran since boot (the first WiFi.mode(STA) is the slow one)
bool send(const uint8_t *mac, const uint8_t *data, uint8_t len);  // mac == nullptr -> broadcast
bool receive(PvpDiagRx &out);        // non-blocking
uint32_t txCallbackFails();          // send-callback failures since start()
uint32_t queueDrops();               // frames lost to a full receive queue since start()
}  // namespace PvpDiagRadio

#endif  // TAMAPOKE_LOCAL_TEST
