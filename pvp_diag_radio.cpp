#ifdef TAMAPOKE_LOCAL_TEST

#include "pvp_diag_radio.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <assert.h>
#include <freertos/queue.h>
#include <string.h>

static_assert(PVP_MAX_PACKET <= ESP_NOW_MAX_DATA_LEN, "PvP packet must fit one ESP-NOW frame");

namespace {
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Static queue: its handle lives for the whole program, so a late WiFi-task callback
// can never touch a destroyed queue; start() empties it.
uint8_t gRxStorage[PVP_RX_QUEUE_LEN * sizeof(PvpDiagRx)];
StaticQueue_t gRxQueueCb;
QueueHandle_t gRxQueue = nullptr;
bool gRunning = false;
bool gEverStarted = false;
volatile uint32_t gTxFails = 0;
volatile uint32_t gDrops = 0;

// WiFi-task context: copy into the queue and nothing else (no Serial, NVS, SD, logic).
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  assert(!xPortInIsrContext());  // runs in the WiFi task, never in an ISR
  PvpDiagRx rx;
  if (!info || !pvpInPacketFrom(info->src_addr, data, len, rx.packet)) return;
  rx.rssi = info->rx_ctrl ? (int8_t)info->rx_ctrl->rssi : 0;
  if (xQueueSend(gRxQueue, &rx, 0) != pdTRUE) gDrops = gDrops + 1;
}

void onSend(const esp_now_send_info_t *, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) gTxFails = gTxFails + 1;
}

bool ensurePeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;  // current channel
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}
}  // namespace

PvpDiagHeap pvpDiagHeap() {
  PvpDiagHeap h;
  h.freeHeap = ESP.getFreeHeap();
  h.minFreeHeap = ESP.getMinFreeHeap();
  h.maxBlock = ESP.getMaxAllocHeap();
  return h;
}

bool PvpDiagRadio::start(uint8_t outMac[6]) {
  if (gRunning) return esp_wifi_get_mac(WIFI_IF_STA, outMac) == ESP_OK;
  gEverStarted = true;
  WiFi.persistent(false);               // before mode(): keep config out of NVS
  WiFi.setSleep(WIFI_PS_NONE);          // before mode(): STA_START would re-apply getSleep()
  if (!WiFi.mode(WIFI_STA)) return false;
  if (esp_wifi_set_channel(PVP_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) { stop(); return false; }
  if (esp_wifi_get_mac(WIFI_IF_STA, outMac) != ESP_OK) { stop(); return false; }
  if (!gRxQueue) gRxQueue = xQueueCreateStatic(PVP_RX_QUEUE_LEN, sizeof(PvpDiagRx), gRxStorage, &gRxQueueCb);
  xQueueReset(gRxQueue);
  gTxFails = 0;
  gDrops = 0;
  if (esp_now_init() != ESP_OK) { stop(); return false; }
  gRunning = true;  // from here stop() also deinitialises ESP-NOW
  if (esp_now_register_recv_cb(onRecv) != ESP_OK || esp_now_register_send_cb(onSend) != ESP_OK ||
      !ensurePeer(BROADCAST_MAC)) {
    stop();
    return false;
  }
  return true;
}

void PvpDiagRadio::stop() {
  if (gRunning) {
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    gRunning = false;
  }
  WiFi.mode(WIFI_OFF);
}

bool PvpDiagRadio::running() { return gRunning; }
bool PvpDiagRadio::everStarted() { return gEverStarted; }

bool PvpDiagRadio::send(const uint8_t *mac, const uint8_t *data, uint8_t len) {
  if (!gRunning || !data || len == 0 || len > PVP_MAX_PACKET) return false;
  const uint8_t *dst = mac ? mac : BROADCAST_MAC;
  if (mac && !ensurePeer(mac)) return false;
  return esp_now_send(dst, data, len) == ESP_OK;
}

bool PvpDiagRadio::receive(PvpDiagRx &out) {
  return gRxQueue && xQueueReceive(gRxQueue, &out, 0) == pdTRUE;
}

uint32_t PvpDiagRadio::txCallbackFails() { return gTxFails; }
uint32_t PvpDiagRadio::queueDrops() { return gDrops; }

#endif  // TAMAPOKE_LOCAL_TEST
