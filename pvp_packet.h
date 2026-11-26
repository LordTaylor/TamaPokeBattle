#pragma once

#include <stddef.h>
#include <stdint.h>

#include "battle.h"
#include "pvp_config.h"

enum PvpMsgType : uint8_t {
  PVP_MSG_HELLO = 1,
  PVP_MSG_CHALLENGE = 2,
  PVP_MSG_ACCEPT = 3,
  PVP_MSG_DECLINE = 4,
  PVP_MSG_READY = 5,
  PVP_MSG_ACTION = 6,
  PVP_MSG_ROUND_HASH = 7,
  PVP_MSG_ACK = 8,
  PVP_MSG_ABORT = 9,
  PVP_MSG_BYE = 10,
};

enum PvpReason : uint8_t {
  PVP_REASON_NONE = 0,
  PVP_REASON_USER = 1,
  PVP_REASON_VERSION = 2,
  PVP_REASON_TIMEOUT = 3,
  PVP_REASON_DESYNC = 4,
  PVP_REASON_PROTOCOL = 5,
  PVP_REASON_BUSY = 6,
};
constexpr uint8_t PVP_REASON_MAX = PVP_REASON_BUSY;

enum PvpParseResult : uint8_t {
  PVP_PARSE_OK = 0,
  PVP_PARSE_TOO_SHORT,
  PVP_PARSE_BAD_MAGIC,
  PVP_PARSE_BAD_LENGTH,
  PVP_PARSE_BAD_CRC,
  PVP_PARSE_BAD_VERSION,  // header fields (type, session) are still filled in
  PVP_PARSE_BAD_TYPE,
  PVP_PARSE_BAD_VALUE,
};

struct PvpReadyPayload {
  uint16_t dex = 0;
  uint8_t shiny = 0;
  uint8_t level = 0;
  uint16_t hp = 0;
  uint16_t atk = 0;
  uint16_t def = 0;
  uint16_t spe = 0;
  uint8_t type1 = 0;
  uint8_t type2 = 0;
};

// Decoded message. Only the fields of the given type are meaningful.
struct PvpMessage {
  uint8_t proto = PVP_PROTO_VERSION;
  uint8_t type = 0;
  uint32_t session = 0;
  uint16_t seq = 0;
  uint32_t deviceId = 0;           // HELLO, CHALLENGE, ACCEPT
  uint32_t nonce = 0;              // CHALLENGE, ACCEPT
  char nick[PVP_NICK_BUF] = {0};   // HELLO
  uint8_t trainerLevel = 0;        // HELLO
  uint8_t version = PVP_PROTO_VERSION;  // HELLO
  uint8_t busy = 0;                // HELLO
  PvpReadyPayload ready;           // READY
  uint16_t round = 0;              // ACTION, ROUND_HASH
  uint8_t action = 0;              // ACTION
  uint32_t hash = 0;               // ROUND_HASH
  uint8_t finalRound = 0;          // ROUND_HASH
  uint16_t ackSeq = 0;             // ACK
  uint8_t reason = 0;              // ABORT
};

uint16_t pvpCrc16(const uint8_t *data, size_t len);  // CRC-16/CCITT-FALSE

// Payload size for a type, or -1 if the type is unknown.
int pvpPayloadSize(uint8_t type);

bool pvpValidNick(const char nick[PVP_NICK_BUF]);
bool pvpValidReady(const PvpReadyPayload &ready);

// Writes the packet (little-endian, CRC filled in). Returns the byte count,
// or 0 when the values are invalid or the buffer is too small.
size_t pvpSerialize(const PvpMessage &msg, uint8_t *out, size_t cap);

// Validates and decodes. `out` is only written on OK and BAD_VERSION.
PvpParseResult pvpParse(const uint8_t *data, size_t len, PvpMessage &out);

// Recomputes the CRC field of an already laid out packet (used by tests/tools).
void pvpSealCrc(uint8_t *packet, size_t len);

BattleStats pvpToBattleStats(const PvpReadyPayload &ready);

// Deterministic luck 0..99 for a round; nonces ordered by side (A = lower deviceId).
uint8_t pvpLuck(uint32_t nonceA, uint32_t nonceB, uint16_t round);
