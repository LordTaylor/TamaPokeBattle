#pragma once

#include <stdint.h>

#include "pvp_in_packet.h"
#include "pvp_out_packet.h"

// Transport behind PvpApp: ESP-NOW on the device, a fake in tests.
// All methods are called from loop() only.
class PvpLink {
 public:
  virtual ~PvpLink() {}
  // Powers the radio up; writes the own STA MAC. False = radio failure (already cleaned up).
  virtual bool start(uint8_t outMac[6]) = 0;
  virtual void stop() = 0;
  virtual bool running() const = 0;
  // False when the packet could not be queued; the session layer retransmits.
  virtual bool send(const PvpOutPacket &packet) = 0;
  virtual bool receive(PvpInPacket &packet) = 0;
  // Packets handed to the radio and not yet reported as sent; stop() waits for 0 (bounded).
  virtual uint8_t inFlight() const { return 0; }
};
