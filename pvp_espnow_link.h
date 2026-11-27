#pragma once

#include "pvp_link.h"

// ESP-NOW transport (firmware only; host tests use a fake PvpLink).
// The radio callback only copies the frame into a static FreeRTOS queue; every
// protocol decision happens in loop() via receive().
class PvpEspNowLink : public PvpLink {
 public:
  bool start(uint8_t outMac[6]) override;
  void stop() override;
  bool running() const override { return running_; }
  bool send(const PvpOutPacket &packet) override;
  bool receive(PvpInPacket &packet) override;
  uint8_t inFlight() const override;

 private:
  bool ensurePeer(const uint8_t *mac);
  bool running_ = false;
};
