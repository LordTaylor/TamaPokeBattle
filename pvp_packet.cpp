#include "pvp_packet.h"

#include <string.h>

namespace {

constexpr size_t OFF_MAGIC = 0;
constexpr size_t OFF_PROTO = 2;
constexpr size_t OFF_TYPE = 3;
constexpr size_t OFF_SESSION = 4;
constexpr size_t OFF_SEQ = 8;
constexpr size_t OFF_LEN = 10;
constexpr size_t OFF_CRC = 11;

void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

uint16_t get16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void writeReady(uint8_t *p, const PvpReadyPayload &r) {
  put16(p, r.dex);
  p[2] = r.shiny;
  p[3] = r.level;
  put16(p + 4, r.hp);
  put16(p + 6, r.atk);
  put16(p + 8, r.def);
  put16(p + 10, r.spe);
  p[12] = r.type1;
  p[13] = r.type2;
}

void readReady(const uint8_t *p, PvpReadyPayload &r) {
  r.dex = get16(p);
  r.shiny = p[2];
  r.level = p[3];
  r.hp = get16(p + 4);
  r.atk = get16(p + 6);
  r.def = get16(p + 8);
  r.spe = get16(p + 10);
  r.type1 = p[12];
  r.type2 = p[13];
}

bool validMessage(const PvpMessage &m) {
  switch (m.type) {
    case PVP_MSG_HELLO: return pvpValidNick(m.nick) && m.busy <= 1;
    case PVP_MSG_READY: return pvpValidReady(m.ready);
    case PVP_MSG_ACTION: return m.action < PVP_ACTION_COUNT;
    case PVP_MSG_ROUND_HASH: return m.finalRound <= 1;
    case PVP_MSG_ABORT: return m.reason <= PVP_REASON_MAX;
    default: return true;
  }
}

void writePayload(const PvpMessage &m, uint8_t *p) {
  switch (m.type) {
    case PVP_MSG_HELLO:
      put32(p, m.deviceId);
      memcpy(p + 4, m.nick, PVP_NICK_BUF);
      p[4 + PVP_NICK_BUF] = m.trainerLevel;
      p[5 + PVP_NICK_BUF] = m.version;
      p[6 + PVP_NICK_BUF] = m.busy;
      break;
    case PVP_MSG_CHALLENGE:
    case PVP_MSG_ACCEPT:
      put32(p, m.deviceId);
      put32(p + 4, m.nonce);
      break;
    case PVP_MSG_READY: writeReady(p, m.ready); break;
    case PVP_MSG_ACTION:
      put16(p, m.round);
      p[2] = m.action;
      break;
    case PVP_MSG_ROUND_HASH:
      put16(p, m.round);
      put32(p + 2, m.hash);
      p[6] = m.finalRound;
      break;
    case PVP_MSG_ACK: put16(p, m.ackSeq); break;
    case PVP_MSG_ABORT: p[0] = m.reason; break;
    default: break;
  }
}

void readPayload(const uint8_t *p, PvpMessage &m) {
  switch (m.type) {
    case PVP_MSG_HELLO:
      m.deviceId = get32(p);
      memcpy(m.nick, p + 4, PVP_NICK_BUF);
      m.trainerLevel = p[4 + PVP_NICK_BUF];
      m.version = p[5 + PVP_NICK_BUF];
      m.busy = p[6 + PVP_NICK_BUF];
      break;
    case PVP_MSG_CHALLENGE:
    case PVP_MSG_ACCEPT:
      m.deviceId = get32(p);
      m.nonce = get32(p + 4);
      break;
    case PVP_MSG_READY: readReady(p, m.ready); break;
    case PVP_MSG_ACTION:
      m.round = get16(p);
      m.action = p[2];
      break;
    case PVP_MSG_ROUND_HASH:
      m.round = get16(p);
      m.hash = get32(p + 2);
      m.finalRound = p[6];
      break;
    case PVP_MSG_ACK: m.ackSeq = get16(p); break;
    case PVP_MSG_ABORT: m.reason = p[0]; break;
    default: break;
  }
}

uint32_t fnvStep(uint32_t h, uint32_t value, int bytes) {
  for (int i = 0; i < bytes; i++) {
    h ^= (uint8_t)(value >> (8 * i));
    h *= 16777619u;
  }
  return h;
}

}  // namespace

uint16_t pvpCrc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)((uint16_t)data[i] << 8);
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

int pvpPayloadSize(uint8_t type) {
  switch (type) {
    case PVP_MSG_HELLO: return PVP_LEN_HELLO;
    case PVP_MSG_CHALLENGE:
    case PVP_MSG_ACCEPT: return PVP_LEN_CHALLENGE;
    case PVP_MSG_DECLINE: return PVP_LEN_DECLINE;
    case PVP_MSG_READY: return PVP_LEN_READY;
    case PVP_MSG_ACTION: return PVP_LEN_ACTION;
    case PVP_MSG_ROUND_HASH: return PVP_LEN_ROUND_HASH;
    case PVP_MSG_ACK: return PVP_LEN_ACK;
    case PVP_MSG_ABORT: return PVP_LEN_ABORT;
    case PVP_MSG_BYE: return PVP_LEN_BYE;
    default: return -1;
  }
}

bool pvpValidNick(const char nick[PVP_NICK_BUF]) {
  size_t n = 0;
  while (n < PVP_NICK_BUF && nick[n] != '\0') {
    char c = nick[n];
    if (!((c >= 'A' && c <= 'Z') || c == '.' || c == '-')) return false;
    n++;
  }
  if (n == 0 || n > PVP_NICK_MAX) return false;
  for (size_t i = n; i < PVP_NICK_BUF; i++) {
    if (nick[i] != '\0') return false;  // canonical: zero padding only
  }
  return true;
}

bool pvpValidReady(const PvpReadyPayload &r) {
  if (r.dex < PVP_DEX_MIN || r.dex > PVP_DEX_MAX) return false;
  if (r.shiny > 1) return false;
  if (r.level < PVP_LEVEL_MIN || r.level > PVP_LEVEL_MAX) return false;
  const uint16_t stats[4] = { r.hp, r.atk, r.def, r.spe };
  for (uint16_t s : stats) {
    if (s < PVP_STAT_MIN || s > PVP_STAT_MAX) return false;
  }
  return r.type1 <= PVP_TYPE_MAX && r.type2 <= PVP_TYPE_MAX;
}

void pvpSealCrc(uint8_t *packet, size_t len) {
  if (len < PVP_HEADER_SIZE) return;
  put16(packet + OFF_CRC, 0);
  put16(packet + OFF_CRC, pvpCrc16(packet, len));
}

size_t pvpSerialize(const PvpMessage &msg, uint8_t *out, size_t cap) {
  int payload = pvpPayloadSize(msg.type);
  if (payload < 0 || !validMessage(msg)) return 0;
  size_t total = PVP_HEADER_SIZE + (size_t)payload;
  if (cap < total || total > PVP_MAX_PACKET) return 0;
  memset(out, 0, total);
  put16(out + OFF_MAGIC, PVP_MAGIC);
  out[OFF_PROTO] = PVP_PROTO_VERSION;
  out[OFF_TYPE] = msg.type;
  put32(out + OFF_SESSION, msg.session);
  put16(out + OFF_SEQ, msg.seq);
  out[OFF_LEN] = (uint8_t)payload;
  writePayload(msg, out + PVP_HEADER_SIZE);
  pvpSealCrc(out, total);
  return total;
}

PvpParseResult pvpParse(const uint8_t *data, size_t len, PvpMessage &out) {
  if (len < PVP_HEADER_SIZE) return PVP_PARSE_TOO_SHORT;
  if (get16(data + OFF_MAGIC) != PVP_MAGIC) return PVP_PARSE_BAD_MAGIC;
  if (len > PVP_MAX_PACKET || len != (size_t)PVP_HEADER_SIZE + data[OFF_LEN]) {
    return PVP_PARSE_BAD_LENGTH;
  }

  uint8_t copy[PVP_MAX_PACKET];
  memcpy(copy, data, len);
  uint16_t wireCrc = get16(copy + OFF_CRC);
  put16(copy + OFF_CRC, 0);
  if (pvpCrc16(copy, len) != wireCrc) return PVP_PARSE_BAD_CRC;

  PvpMessage m;
  m.proto = data[OFF_PROTO];
  m.type = data[OFF_TYPE];
  m.session = get32(data + OFF_SESSION);
  m.seq = get16(data + OFF_SEQ);
  if (m.proto != PVP_PROTO_VERSION) {
    out = m;
    return PVP_PARSE_BAD_VERSION;
  }
  int expected = pvpPayloadSize(m.type);
  if (expected < 0) return PVP_PARSE_BAD_TYPE;
  if (data[OFF_LEN] != expected) return PVP_PARSE_BAD_LENGTH;
  readPayload(data + PVP_HEADER_SIZE, m);
  if (!validMessage(m)) return PVP_PARSE_BAD_VALUE;
  out = m;
  return PVP_PARSE_OK;
}

BattleStats pvpToBattleStats(const PvpReadyPayload &ready) {
  BattleStats s;
  s.hp = ready.hp;
  s.atk = ready.atk;
  s.def = ready.def;
  s.spe = ready.spe;
  s.level = ready.level;
  s.type1 = ready.type1;
  s.type2 = ready.type2;
  return s;
}

uint8_t pvpLuck(uint32_t nonceA, uint32_t nonceB, uint16_t round) {
  uint32_t h = 2166136261u;
  h = fnvStep(h, nonceA, 4);
  h = fnvStep(h, nonceB, 4);
  h = fnvStep(h, round, 2);
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return (uint8_t)(h % PVP_LUCK_MOD);
}
