#pragma once

#include <stdint.h>

// PvP protocol constants (single source of truth). Pure logic, no Arduino deps.

// --- Wire format ---
constexpr uint16_t PVP_MAGIC = 0x5450;
constexpr uint8_t PVP_PROTO_VERSION = 2;  // 2: CHALLENGE/ACCEPT carry nick+level, HELLO has no busy flag
constexpr uint8_t PVP_HEADER_SIZE = 13;     // magic2 proto1 type1 session4 seq2 len1 crc2
constexpr uint8_t PVP_ESPNOW_MAX = 250;     // ESP-NOW payload limit
constexpr uint8_t PVP_MAX_PACKET = 48;      // our largest packet (header + payload) fits here

constexpr uint8_t PVP_NICK_LEN = 10;        // characters, without NUL
constexpr uint8_t PVP_NICK_BUF = PVP_NICK_LEN + 1;
constexpr char PVP_DEFAULT_NICK[] = "PLAYER";  // used when the local nick is invalid

// Fixed payload sizes per message type.
constexpr uint8_t PVP_LEN_HELLO = 4 + PVP_NICK_BUF + 2;  // id, nick, trainer level, version
constexpr uint8_t PVP_LEN_CHALLENGE = 8 + PVP_NICK_BUF + 1;  // id, nonce, nick, trainer level (also ACCEPT)
constexpr uint8_t PVP_LEN_DECLINE = 0;
constexpr uint8_t PVP_LEN_READY = 14;
constexpr uint8_t PVP_LEN_ACTION = 3;                    // round u16, action u8
constexpr uint8_t PVP_LEN_ROUND_HASH = 7;                // round u16, hash u32, final u8
constexpr uint8_t PVP_LEN_ACK = 2;
constexpr uint8_t PVP_LEN_ABORT = 1;
constexpr uint8_t PVP_LEN_BYE = 0;

// --- Value ranges (validated on both parse and serialize) ---
constexpr uint16_t PVP_DEX_MIN = 1;
constexpr uint16_t PVP_DEX_MAX = 386;       // DEX_COUNT in dex.h
constexpr uint8_t PVP_LEVEL_MIN = 1;
constexpr uint8_t PVP_LEVEL_MAX = 100;
constexpr uint16_t PVP_STAT_MIN = 1;        // atk/def/spe
constexpr uint16_t PVP_STAT_MAX = 999;      // atk/def/spe; game maximum is about 453
// HP may be 0 on the wire: "receiver derives it" (battle engine hpFor/turnHpFor).
// Game maximum of the derived turn HP is 983 (30 + 5 * level + def); the battle engine
// accepts 0..999, so the wire limit matches it.
constexpr uint16_t PVP_HP_MAX = 999;
constexpr uint8_t PVP_TYPE_MAX = 18;        // TYPE_FAIRY in dex.h (0 = none)
constexpr uint8_t PVP_ACTION_COUNT = 5;     // BattleAction 0..4
constexpr uint8_t PVP_DEFAULT_ACTION = 0;   // BATTLE_ATTACK

// --- Timing ---
constexpr uint32_t PVP_HELLO_INTERVAL_MS = 1000;
constexpr uint32_t PVP_PEER_EXPIRE_MS = 3000;
constexpr uint32_t PVP_RETX_INTERVAL_MS = 150;
constexpr uint8_t PVP_RETX_FAST_COUNT = 8;      // fast retransmits after the first send
constexpr uint32_t PVP_RETX_SLOW_MS = 1000;     // afterwards, until the silence timeout
constexpr uint32_t PVP_PEER_TIMEOUT_MS = 10000; // no valid packet in a session -> ABORTED
constexpr uint32_t PVP_ACTION_TIMEOUT_MS = 20000;
constexpr uint32_t PVP_KEEPALIVE_MS = 2000;
constexpr uint32_t PVP_CHALLENGE_TIMEOUT_MS = 30000;  // human decision window
constexpr uint32_t PVP_STALL_MS = 60000;              // no state progress at all
constexpr uint32_t PVP_LINGER_MS = PVP_PEER_TIMEOUT_MS;  // FINISHED keeps retransmitting this long
constexpr uint32_t PVP_REPLY_MIN_INTERVAL_MS = 250;   // rate limit for unsolicited DECLINE/VERSION replies

// --- Capacities (static buffers, no heap) ---
constexpr uint8_t PVP_MAX_PEERS = 8;
constexpr uint8_t PVP_MAX_PENDING = 8;
constexpr uint8_t PVP_SEND_QUEUE = 16;
constexpr uint8_t PVP_EVENT_QUEUE = 16;
constexpr uint8_t PVP_FIRE_COPIES = 3;      // copies of unacknowledged ABORT/DECLINE/BYE
constexpr uint8_t PVP_SEQ_WINDOW = 32;
constexpr uint8_t PVP_LUCK_MOD = 100;

// --- Transport (ESP-NOW) ---
constexpr uint8_t PVP_WIFI_CHANNEL = 1;       // fixed channel for both devices, set after every WiFi start
constexpr uint8_t PVP_RX_QUEUE_LEN = 16;      // static receive queue (callback only copies packets into it)
