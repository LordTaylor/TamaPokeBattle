#pragma once

// T4.0 measurement tooling (TAMAPOKE_LOCAL_TEST only): every threshold and tunable in one place.
// Gate values come from docs/pvp-integration-spec.md A.3a / B.8.

#include <stdint.h>

// --- Ping test (PVPPING) ---
constexpr uint16_t PVP_DIAG_PING_TOTAL = 1000;          // packets per direction, alternating broadcast/unicast
constexpr uint32_t PVP_DIAG_PING_INTERVAL_MS = 20;      // 1000 packets in 20 s
constexpr uint32_t PVP_DIAG_DISCOVER_MS = 30000;        // wait for the other board before giving up
constexpr uint32_t PVP_DIAG_HELLO_INTERVAL_MS = 100;
constexpr uint32_t PVP_DIAG_TAIL_MS = 5000;             // wait for the peer's END after our last packet
constexpr uint8_t PVP_DIAG_END_COPIES = 5;
constexpr uint32_t PVP_DIAG_END_GAP_MS = 50;
constexpr uint32_t PVP_DIAG_AUDIO_RETRIGGER_MS = 350;   // re-queues a sound effect so audio plays throughout
constexpr uint8_t PVP_DIAG_LOSS_MAX_PCT = 5;            // gate without audio
constexpr uint8_t PVP_DIAG_LOSS_MAX_PCT_AUDIO = 10;     // gate with audio playing
constexpr uint32_t PVP_DIAG_PROGRESS_EVERY = 200;       // log a progress line every N packets
constexpr uint16_t PVP_DIAG_WIRE_LEN = 12;              // magic2 tag1 kind1 id4 seq2 total2
constexpr uint8_t PVP_DIAG_TAG = 0xD0;                  // marks diagnostic frames (byte after the magic)
constexpr uint32_t PVP_DIAG_SERIAL_TX_TIMEOUT_MS = 1500;  // like SAVEGET: do not drop report lines

// --- Radio cycles (RADIOCYC) ---
constexpr uint8_t PVP_DIAG_CYCLES = 10;
constexpr uint32_t PVP_DIAG_HEAP_LEAK_TOL = 2048;       // bytes: cycle N free heap may be this far below cycle 1
constexpr uint32_t PVP_DIAG_MAX_BLOCK_MIN = 60 * 1024;  // getMaxAllocHeap() after start() must be >= this
constexpr uint32_t PVP_DIAG_FIRST_START_MAX_MS = 1500;  // first WiFi.mode(STA) after boot
constexpr uint32_t PVP_DIAG_CYCLE_PAUSE_MS = 100;

// --- NVS ---
constexpr uint8_t PVP_DIAG_NVS_FREE_MIN_PCT = 55;       // free entries after cycle 10, percent of total
constexpr uint8_t PVP_DIAG_NVS_MAX_NAMESPACES = 24;
constexpr char PVP_DIAG_NVS_PARTITION[] = "nvs";        // partition label in app3M_fat9M_16MB.csv

// --- Geometry screen (GEOM) ---
constexpr int PVP_DIAG_GEOM_CENTER = 233;
constexpr int PVP_DIAG_GEOM_RADII[3] = {225, 233, 240};
constexpr int PVP_DIAG_GEOM_RULER_Y[3] = {30, 60, 430};
constexpr int PVP_DIAG_GEOM_TICK_STEP = 20;
constexpr int PVP_DIAG_GEOM_SIZE = 466;
constexpr uint32_t PVP_DIAG_GEOM_TIMEOUT_MS = 300000;   // screen closes itself after 5 minutes
