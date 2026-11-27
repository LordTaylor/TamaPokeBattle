#ifdef TAMAPOKE_LOCAL_TEST

#include <Arduino.h>

#include "pvp_diag.h"
#include "pvp_diag_config.h"
#include "pvp_diag_nvs.h"
#include "pvp_diag_radio.h"

// RADIOCYC: start/stop timing and PVP_DIAG_CYCLES full bring-up/tear-down cycles
// with heap and NVS logging. Gates (spec A.3a/B.8): cycle N free heap not below cycle 1
// by more than the tolerance, largest free block >= 60 KB after every start, NVS free
// entries >= 55 % after the last cycle, first start < 1.5 s.
void pvpDiagCycles() {
  Serial.setTxTimeoutMs(PVP_DIAG_SERIAL_TX_TIMEOUT_MS);
  bool cold = !PvpDiagRadio::everStarted();
  Serial.printf("RADIOCYC begin cold_boot=%s (use right after reset for a true first-start time)\n", cold ? "yes" : "NO");
  PvpDiagHeap h0 = pvpDiagHeap();
  Serial.printf("RADIOCYC before_wifi free=%u min=%u maxblock=%u\n", (unsigned)h0.freeHeap, (unsigned)h0.minFreeHeap,
                (unsigned)h0.maxBlock);
  pvpDiagNvsPrint("before_wifi");

  uint32_t firstFree = 0;
  uint32_t lastFree = 0;
  uint32_t minBlock = 0xFFFFFFFFu;
  uint32_t firstStartMs = 0;
  bool startFailed = false;
  for (uint8_t i = 1; i <= PVP_DIAG_CYCLES; i++) {
    uint8_t mac[6] = {0};
    uint32_t t0 = millis();
    bool ok = PvpDiagRadio::start(mac);
    uint32_t startMs = millis() - t0;
    PvpDiagHeap up = pvpDiagHeap();
    uint32_t t1 = millis();
    PvpDiagRadio::stop();
    uint32_t stopMs = millis() - t1;
    delay(PVP_DIAG_CYCLE_PAUSE_MS);  // let WiFi/netif events settle before reading the heap
    PvpDiagHeap down = pvpDiagHeap();
    Serial.printf("RADIOCYC cycle=%u start=%s start_ms=%u stop_ms=%u up: free=%u min=%u maxblock=%u | down: free=%u min=%u maxblock=%u\n",
                  i, ok ? "ok" : "FAIL", (unsigned)startMs, (unsigned)stopMs, (unsigned)up.freeHeap, (unsigned)up.minFreeHeap,
                  (unsigned)up.maxBlock, (unsigned)down.freeHeap, (unsigned)down.minFreeHeap, (unsigned)down.maxBlock);
    if (!ok) { startFailed = true; break; }
    if (i == 1) {
      firstFree = down.freeHeap;
      firstStartMs = startMs;
      pvpDiagNvsPrint("after_first_start");
    }
    lastFree = down.freeHeap;
    if (up.maxBlock < minBlock) minBlock = up.maxBlock;
  }
  PvpDiagNvsStat nvs = pvpDiagNvsPrint("after_cycles");

  bool leakOk = !startFailed && lastFree + PVP_DIAG_HEAP_LEAK_TOL >= firstFree;
  bool blockOk = !startFailed && minBlock >= PVP_DIAG_MAX_BLOCK_MIN;
  bool nvsOk = nvs.ok && nvs.availPct >= PVP_DIAG_NVS_FREE_MIN_PCT;
  bool firstOk = cold ? firstStartMs < PVP_DIAG_FIRST_START_MAX_MS : true;
  Serial.printf("RADIOCYC gate heap_leak(free after stop: cycleN>=cycle1-%u): %s (c1=%u cN=%u)\n", (unsigned)PVP_DIAG_HEAP_LEAK_TOL,
                leakOk ? "PASS" : "FAIL", (unsigned)firstFree, (unsigned)lastFree);
  Serial.printf("RADIOCYC gate max_block>=%u: %s (min over cycles=%u)\n", (unsigned)PVP_DIAG_MAX_BLOCK_MIN,
                blockOk ? "PASS" : "FAIL", (unsigned)(minBlock == 0xFFFFFFFFu ? 0 : minBlock));
  Serial.printf("RADIOCYC gate nvs_available>=%u%% of usable: %s (avail%%=%u, free%%=%u incl. reserved page)\n",
                (unsigned)PVP_DIAG_NVS_FREE_MIN_PCT, nvsOk ? "PASS" : "FAIL", (unsigned)nvs.availPct, (unsigned)nvs.freePct);
  Serial.printf("RADIOCYC gate first_start (whole start() incl. ESP-NOW init)<%ums: %s (%ums%s)\n", (unsigned)PVP_DIAG_FIRST_START_MAX_MS,
                cold ? (firstOk ? "PASS" : "FAIL") : "N/A", (unsigned)firstStartMs, cold ? "" : ", not a cold boot");
  Serial.println("DONE");
  Serial.flush();
  Serial.setTxTimeoutMs(0);
}

#endif
