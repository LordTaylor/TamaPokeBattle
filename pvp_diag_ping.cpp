#ifdef TAMAPOKE_LOCAL_TEST

#include <Arduino.h>
#include <string.h>

#include "audio.h"
#include "pvp_diag.h"
#include "pvp_diag_config.h"
#include "pvp_diag_radio.h"
#include "pvp_packet.h"

// PVPPING: run the same command on two boards. Phase 1 exchanges HELLO frames until both
// sides know each other, phase 2 sends PVP_DIAG_PING_TOTAL frames (even index broadcast,
// odd index unicast) one per PVP_DIAG_PING_INTERVAL_MS, phase 3 swaps END frames and
// prints the report. Frames start with PVP_MAGIC, so they pass the production filter
// pvpInPacketFrom() unchanged.

namespace {
enum DiagKind : uint8_t { K_HELLO = 1, K_BCAST = 2, K_UNI = 3, K_END = 4, K_HELLO_HEARD = 5 };

struct Stats {
  uint16_t received = 0;
  int32_t rssiSum = 0;
  int8_t rssiMin = 127;
  int8_t rssiMax = -128;
  uint8_t seen[(PVP_DIAG_PING_TOTAL + 7) / 8] = {0};

  void add(uint16_t seq, int8_t rssi) {
    if (seq >= PVP_DIAG_PING_TOTAL) return;
    uint8_t bit = (uint8_t)(1u << (seq & 7));
    if (seen[seq >> 3] & bit) return;  // duplicate
    seen[seq >> 3] |= bit;
    received++;
    rssiSum += rssi;
    if (rssi < rssiMin) rssiMin = rssi;
    if (rssi > rssiMax) rssiMax = rssi;
  }
};

struct PingState {
  uint8_t ownMac[6] = {0};
  uint32_t ownId = 0;
  uint8_t peerMac[6] = {0};
  bool peerKnown = false;
  bool peerHeardUs = false;
  bool peerEnded = false;
  uint16_t peerTotal = 0;
  int maxSeq = -1;
  uint32_t idMismatch = 0;
  uint32_t foreign = 0;  // diag frames from a third device
  uint32_t txOk = 0;
  uint32_t txFail = 0;
  Stats bcast;
  Stats uni;
};

PingState S;

bool aborted() {
  if (!Serial.available()) return false;
  while (Serial.available()) Serial.read();
  return true;
}

uint8_t encode(uint8_t *buf, uint8_t kind, uint16_t seq, uint16_t total) {
  buf[0] = (uint8_t)(PVP_MAGIC & 0xFF);
  buf[1] = (uint8_t)(PVP_MAGIC >> 8);
  buf[2] = PVP_DIAG_TAG;
  buf[3] = kind;
  for (uint8_t i = 0; i < 4; i++) buf[4 + i] = (uint8_t)(S.ownId >> (8 * i));
  buf[8] = (uint8_t)seq;
  buf[9] = (uint8_t)(seq >> 8);
  buf[10] = (uint8_t)total;
  buf[11] = (uint8_t)(total >> 8);
  return PVP_DIAG_WIRE_LEN;
}

void transmit(bool broadcast, uint8_t kind, uint16_t seq, uint16_t total) {
  uint8_t buf[PVP_DIAG_WIRE_LEN];
  uint8_t n = encode(buf, kind, seq, total);
  if (PvpDiagRadio::send(broadcast ? nullptr : S.peerMac, buf, n)) S.txOk++;
  else S.txFail++;
}

void handle(const PvpDiagRx &rx) {
  const PvpInPacket &p = rx.packet;
  if (p.len < PVP_DIAG_WIRE_LEN || p.data[2] != PVP_DIAG_TAG) return;
  if (S.peerKnown && memcmp(p.mac, S.peerMac, 6) != 0) { S.foreign++; return; }
  uint32_t id = (uint32_t)p.data[4] | ((uint32_t)p.data[5] << 8) | ((uint32_t)p.data[6] << 16) | ((uint32_t)p.data[7] << 24);
  if (!S.peerKnown) {
    memcpy(S.peerMac, p.mac, 6);
    S.peerKnown = true;
    Serial.printf("PVPPING peer mac=%02X:%02X:%02X:%02X:%02X:%02X payload_id=%08X mac_id=%08X %s rssi=%d\n", p.mac[0],
                  p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5], (unsigned)id, (unsigned)pvpDeviceIdFromMac(p.mac),
                  id == pvpDeviceIdFromMac(p.mac) ? "ID_OK" : "ID_MISMATCH", rx.rssi);
  }
  if (id != pvpDeviceIdFromMac(p.mac)) S.idMismatch++;
  uint8_t kind = p.data[3];
  uint16_t seq = (uint16_t)(p.data[8] | (p.data[9] << 8));
  uint16_t total = (uint16_t)(p.data[10] | (p.data[11] << 8));
  if (kind == K_HELLO_HEARD || kind == K_BCAST || kind == K_UNI || kind == K_END) S.peerHeardUs = true;
  if (kind == K_BCAST) S.bcast.add(seq, rx.rssi);
  else if (kind == K_UNI) S.uni.add(seq, rx.rssi);
  else if (kind == K_END) { S.peerEnded = true; S.peerTotal = total; }
  if ((kind == K_BCAST || kind == K_UNI) && (int)seq > S.maxSeq) S.maxSeq = seq;
}

void pump() {
  PvpDiagRx rx;
  while (PvpDiagRadio::receive(rx)) handle(rx);
}

uint32_t lossTenths(uint32_t expected, uint32_t got) {
  if (expected == 0) return 1000;
  if (got > expected) got = expected;
  return (1000u * (expected - got)) / expected;
}

void printStats(const char *name, const Stats &st, uint32_t expected) {
  uint32_t loss = lossTenths(expected, st.received);
  if (st.received)
    Serial.printf("PVPPING %s rx=%u/%u loss=%u.%u%% rssi avg=%d min=%d max=%d\n", name, (unsigned)st.received,
                  (unsigned)expected, (unsigned)(loss / 10), (unsigned)(loss % 10), (int)(st.rssiSum / st.received),
                  st.rssiMin, st.rssiMax);
  else
    Serial.printf("PVPPING %s rx=0/%u loss=%u.%u%% rssi n/a\n", name, (unsigned)expected, (unsigned)(loss / 10),
                  (unsigned)(loss % 10));
}

void finish(bool audio, bool aborted) {
  uint32_t total = S.peerEnded ? S.peerTotal : (uint32_t)(S.maxSeq + 1);
  uint32_t expB = (total + 1) / 2;
  uint32_t expU = total / 2;
  Serial.printf("PVPPING report audio=%s peer_total=%u (%s)\n", audio ? "on" : "off", (unsigned)total,
                S.peerEnded ? "END received" : "END lost, estimated from max seq");
  printStats("broadcast", S.bcast, expB);
  printStats("unicast  ", S.uni, expU);
  uint32_t loss = lossTenths(expB + expU, (uint32_t)S.bcast.received + S.uni.received);
  uint32_t limit = audio ? PVP_DIAG_LOSS_MAX_PCT_AUDIO : PVP_DIAG_LOSS_MAX_PCT;
  Serial.printf("PVPPING tx ok=%u fail=%u send_cb_fail=%u rx_queue_drops=%u id_mismatch=%u foreign=%u\n", (unsigned)S.txOk,
                (unsigned)S.txFail, (unsigned)PvpDiagRadio::txCallbackFails(), (unsigned)PvpDiagRadio::queueDrops(),
                (unsigned)S.idMismatch, (unsigned)S.foreign);
  PvpDiagHeap h = pvpDiagHeap();
  Serial.printf("PVPPING heap free=%u min=%u maxblock=%u\n", (unsigned)h.freeHeap, (unsigned)h.minFreeHeap,
                (unsigned)h.maxBlock);
  bool lossOk = !aborted && total > 0 && loss < limit * 10;
  const char *verdict = aborted ? "ABORTED (no verdict)" : (lossOk ? "PASS" : "FAIL");
  Serial.printf("PVPPING gate loss<%u%% (combined %u.%u%%): %s; ids: %s. Audio crackle: judge by ear.\n", (unsigned)limit,
                (unsigned)(loss / 10), (unsigned)(loss % 10), verdict,
                aborted ? "n/a" : (S.idMismatch == 0 && S.peerKnown ? "PASS" : "FAIL"));
}
}  // namespace

void pvpDiagPing(bool withAudio) {
  // Phases 1-3 must never block on a full USB-CDC buffer (no monitor open on this board):
  // timeout stays at 0 (drop) until the final report.
  Serial.setTxTimeoutMs(0);
  S = PingState();
  if (withAudio) {
    Serial.printf("PVPPING sound mode=%u (3 = full)\n", (unsigned)audioMode());
    if (audioMode() != SOUND_FULL) {
      Serial.println("PVPPING ERR sound mode must be full (Settings -> sound) for the audio run; SFX_BALL_BOUNCE is silent otherwise");
      Serial.println("DONE");
      return;
    }
  }
  if (!PvpDiagRadio::start(S.ownMac)) {
    Serial.println("PVPPING ERR radio start failed");
    Serial.println("DONE");
    return;
  }
  S.ownId = pvpDeviceIdFromMac(S.ownMac);
  Serial.printf("PVPPING start own mac=%02X:%02X:%02X:%02X:%02X:%02X deviceId=%08X channel=%u audio=%s (any serial input aborts)\n",
                S.ownMac[0], S.ownMac[1], S.ownMac[2], S.ownMac[3], S.ownMac[4], S.ownMac[5], (unsigned)S.ownId,
                (unsigned)PVP_WIFI_CHANNEL, withAudio ? "on" : "off");

  uint32_t lastAudio = 0;
  auto audioTick = [&](uint32_t now) {
    if (withAudio && now - lastAudio >= PVP_DIAG_AUDIO_RETRIGGER_MS) {
      lastAudio = now;
      sfxPlay(SFX_BALL_BOUNCE);
    }
  };

  // Phase 1: find the other board.
  uint32_t t0 = millis();
  uint32_t lastHello = 0;
  bool ready = false;
  bool abort = false;
  while (!(abort = aborted()) && millis() - t0 < PVP_DIAG_DISCOVER_MS) {
    uint32_t now = millis();
    if (now - lastHello >= PVP_DIAG_HELLO_INTERVAL_MS) {
      lastHello = now;
      transmit(true, S.peerKnown ? K_HELLO_HEARD : K_HELLO, 0, 0);
    }
    pump();
    if (S.peerKnown && S.peerHeardUs) { ready = true; break; }
    delay(1);
  }
  if (!ready) {
    Serial.printf("PVPPING %s: other board not found\n", abort ? "aborted" : "timeout");
    PvpDiagRadio::stop();
    Serial.println("DONE");
    return;
  }

  // Phase 2: PVP_DIAG_PING_TOTAL frames on an absolute schedule (no drift).
  Serial.println("PVPPING sending");
  uint32_t t1 = millis();
  for (uint16_t i = 0; i < PVP_DIAG_PING_TOTAL && !abort;) {
    uint32_t now = millis();
    if (now - t1 >= (uint32_t)i * PVP_DIAG_PING_INTERVAL_MS) {
      bool bc = (i % 2) == 0;
      transmit(bc, bc ? K_BCAST : K_UNI, i, PVP_DIAG_PING_TOTAL);
      i++;
      if (i % PVP_DIAG_PROGRESS_EVERY == 0)
        Serial.printf("PVPPING progress tx=%u rx=%u\n", (unsigned)i, (unsigned)(S.bcast.received + S.uni.received));
    }
    audioTick(now);
    pump();
    abort = aborted();
    delay(1);
  }

  // Phase 3: END copies, wait for the peer's END.
  for (uint8_t c = 0; c < PVP_DIAG_END_COPIES; c++) {
    transmit(true, K_END, 0, PVP_DIAG_PING_TOTAL);
    uint32_t g = millis();
    while (millis() - g < PVP_DIAG_END_GAP_MS) { pump(); audioTick(millis()); delay(1); }
  }
  uint32_t t2 = millis();
  while (!abort && !S.peerEnded && millis() - t2 < PVP_DIAG_TAIL_MS) {
    audioTick(millis());
    pump();
    abort = aborted();
    delay(1);
  }
  // Late frames may still sit in the queue.
  pump();
  PvpDiagRadio::stop();  // radio off first; the report may block on serial
  Serial.setTxTimeoutMs(PVP_DIAG_SERIAL_TX_TIMEOUT_MS);
  finish(withAudio, abort);
  Serial.println("DONE");
  Serial.flush();
  Serial.setTxTimeoutMs(0);
}

#endif
