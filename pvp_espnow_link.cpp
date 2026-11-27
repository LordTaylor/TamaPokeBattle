#include "pvp_espnow_link.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

static_assert(PVP_MAX_PACKET <= ESP_NOW_MAX_DATA_LEN, "PvP packet exceeds the ESP-NOW payload limit");

namespace {

const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Static queue: its handle stays valid for the whole program run, so a late radio
// callback after stop() can never touch a destroyed queue. start() empties it.
uint8_t gRxStorage[PVP_RX_QUEUE_LEN * sizeof(PvpInPacket)];
StaticQueue_t gRxQueueCb;
QueueHandle_t gRxQueue = nullptr;

// Incremented in loop() (core 1), decremented in the send callback (core 0): needs atomics.
std::atomic<uint8_t> gInFlight{0};

void inFlightDec() {
  uint8_t cur = gInFlight.load();
  while (cur > 0 && !gInFlight.compare_exchange_weak(cur, (uint8_t)(cur - 1))) {
  }
}

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  PvpInPacket packet;
  if (!info || !pvpInPacketFrom(info->src_addr, data, len, packet)) return;
  xQueueSend(gRxQueue, &packet, 0);  // never blocks; a full queue drops the newest frame
}

void onSent(const wifi_tx_info_t *, esp_now_send_status_t) {
  inFlightDec();
}

}  // namespace

bool PvpEspNowLink::start(uint8_t outMac[6]) {
  if (running_) return true;
  WiFi.persistent(false);              // before mode(): keep the config out of NVS
  WiFi.setSleep(WIFI_PS_NONE);         // before mode(): STA_START re-applies getSleep()
  if (!WiFi.mode(WIFI_STA)) return false;
  if (esp_wifi_set_channel(PVP_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {  // not persisted across starts
    WiFi.mode(WIFI_OFF);
    return false;
  }
  if (esp_wifi_get_mac(WIFI_IF_STA, outMac) != ESP_OK) {
    WiFi.mode(WIFI_OFF);
    return false;
  }
  if (!gRxQueue) {
    gRxQueue = xQueueCreateStatic(PVP_RX_QUEUE_LEN, sizeof(PvpInPacket), gRxStorage, &gRxQueueCb);
  }
  xQueueReset(gRxQueue);
  gInFlight.store(0);
  if (esp_now_init() != ESP_OK) {
    WiFi.mode(WIFI_OFF);
    return false;
  }
  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t broadcast = {};
  memcpy(broadcast.peer_addr, BROADCAST_MAC, 6);
  broadcast.channel = 0;  // current channel
  broadcast.ifidx = WIFI_IF_STA;
  broadcast.encrypt = false;
  if (esp_now_add_peer(&broadcast) != ESP_OK) {
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    return false;
  }
  running_ = true;
  return true;
}

void PvpEspNowLink::stop() {
  if (!running_) return;
  running_ = false;
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  gInFlight.store(0);
}

bool PvpEspNowLink::ensurePeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

bool PvpEspNowLink::send(const PvpOutPacket &packet) {
  if (!running_ || packet.len == 0 || packet.len > PVP_MAX_PACKET) return false;
  const uint8_t *target = packet.broadcast ? BROADCAST_MAC : packet.mac;
  if (!packet.broadcast && !ensurePeer(target)) return false;
  gInFlight.fetch_add(1);
  if (esp_now_send(target, packet.data, packet.len) != ESP_OK) {
    inFlightDec();
    return false;
  }
  return true;
}

bool PvpEspNowLink::receive(PvpInPacket &packet) {
  if (!running_ || !gRxQueue) return false;
  return xQueueReceive(gRxQueue, &packet, 0) == pdTRUE;
}

uint8_t PvpEspNowLink::inFlight() const { return gInFlight.load(); }
