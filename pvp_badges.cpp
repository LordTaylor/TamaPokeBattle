#include "pvp_badges.h"

#include <string.h>

namespace {

constexpr uint16_t WINS_MAX = 65535;
constexpr char NICK_FILLER = '?';

void put32(uint8_t *&p, uint32_t v) {
  for (int i = 0; i < 4; i++) *p++ = (uint8_t)(v >> (8 * i));
}
uint32_t get32(const uint8_t *&p) {
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) v |= (uint32_t)(*p++) << (8 * i);
  return v;
}

void cleanNick(char *nick) {
  nick[PVP_NICK_MAX] = 0;
  bool ended = false;
  for (uint8_t i = 0; i < PVP_NICK_MAX; i++) {
    if (ended || nick[i] == 0) {
      ended = true;
      nick[i] = 0;
    } else if (nick[i] < 0x20 || nick[i] > 0x7E) {
      nick[i] = NICK_FILLER;
    }
  }
}

uint16_t addWins(uint16_t a, uint16_t b) {
  uint32_t s = (uint32_t)a + b;
  return s > WINS_MAX ? WINS_MAX : (uint16_t)s;
}

uint32_t maxSeq(const PvpBadge *ring) {
  uint32_t m = 0;
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++) {
    if (ring[i].seq > m) m = ring[i].seq;
  }
  return m;
}

int findDevice(const PvpBadge *ring, uint32_t deviceId) {
  for (int i = 0; i < PVP_BADGE_SLOTS; i++) {
    if (ring[i].seq != 0 && ring[i].deviceId == deviceId) return i;
  }
  return -1;
}

void shiftDown(PvpBadge *ring, int upTo) {  // frees [0], overwriting the entry at index `upTo`
  for (int i = upTo; i > 0; i--) ring[i] = ring[i - 1];
}

}  // namespace

void pvpBadgeRecord(PvpBadge *ring, uint32_t deviceId, const char *nick, size_t nickLen,
                    uint8_t oppLevel, uint32_t nowEpoch, uint32_t seq) {
  if (deviceId == 0) return;
  if (maxSeq(ring) == UINT32_MAX) sanitizePvpBadges(ring);  // renumbers: top + 1 must not wrap to 0
  uint32_t top = maxSeq(ring);
  if (seq <= top) seq = top + 1;
  int at = findDevice(ring, deviceId);
  PvpBadge entry;
  if (at >= 0) {
    entry = ring[at];
    entry.wins = addWins(entry.wins, 1);
  } else {
    entry.deviceId = deviceId;
    entry.firstEpoch = nowEpoch;
    entry.wins = 1;
    at = PVP_BADGE_SLOTS - 1;
  }
  entry.lastEpoch = nowEpoch;
  entry.seq = seq;
  entry.oppLevel = oppLevel;
  memset(entry.nick, 0, sizeof(entry.nick));
  size_t n = nickLen < PVP_NICK_MAX ? nickLen : PVP_NICK_MAX;
  if (nick) memcpy(entry.nick, nick, n);
  cleanNick(entry.nick);
  shiftDown(ring, at);
  ring[0] = entry;
}

void sanitizePvpBadges(PvpBadge *ring) {
  PvpBadge out[PVP_BADGE_SLOTS];
  uint8_t count = 0;
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++) {
    PvpBadge e = ring[i];
    if (e.deviceId == 0 || e.seq == 0) continue;
    cleanNick(e.nick);
    int dup = -1;
    for (uint8_t k = 0; k < count; k++) {
      if (out[k].deviceId == e.deviceId) dup = k;
    }
    if (dup < 0) {
      out[count++] = e;
      continue;
    }
    PvpBadge &d = out[dup];
    uint32_t first = d.firstEpoch;
    if (e.firstEpoch && (first == 0 || e.firstEpoch < first)) first = e.firstEpoch;
    uint32_t last = d.lastEpoch > e.lastEpoch ? d.lastEpoch : e.lastEpoch;
    uint16_t wins = addWins(d.wins, e.wins);
    if (e.seq > d.seq) d = e;  // the newer entry keeps its nick/level/seq
    d.firstEpoch = first;
    d.lastEpoch = last;
    d.wins = wins;
  }
  for (uint8_t i = 1; i < count; i++) {  // stable insertion sort, newest first
    PvpBadge key = out[i];
    int j = i - 1;
    while (j >= 0 && out[j].seq < key.seq) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }
  for (uint8_t i = 0; i < count; i++) out[i].seq = (uint32_t)(count - i);  // 1..count, never 0
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++) ring[i] = i < count ? out[i] : PvpBadge();
}

void pvpBadgesSerialize(const PvpBadge *ring, uint8_t *out) {
  uint8_t *p = out;
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++) {
    const PvpBadge &b = ring[i];
    put32(p, b.deviceId);
    put32(p, b.firstEpoch);
    put32(p, b.lastEpoch);
    put32(p, b.seq);
    *p++ = (uint8_t)(b.wins & 0xFF);
    *p++ = (uint8_t)(b.wins >> 8);
    *p++ = b.oppLevel;
    memcpy(p, b.nick, PVP_NICK_MAX + 1);
    p += PVP_NICK_MAX + 1;
  }
}

void pvpBadgesDeserialize(const uint8_t *blob, size_t len, PvpBadge *ring) {
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++) ring[i] = PvpBadge();
  if (!blob || len != PVP_BADGE_BLOB_BYTES) return;
  const uint8_t *p = blob;
  for (uint8_t i = 0; i < PVP_BADGE_SLOTS; i++) {
    PvpBadge &b = ring[i];
    b.deviceId = get32(p);
    b.firstEpoch = get32(p);
    b.lastEpoch = get32(p);
    b.seq = get32(p);
    b.wins = (uint16_t)(p[0] | (p[1] << 8));
    p += 2;
    b.oppLevel = *p++;
    memcpy(b.nick, p, PVP_NICK_MAX + 1);
    p += PVP_NICK_MAX + 1;
  }
  sanitizePvpBadges(ring);
}

bool pvpBadgeEarned(PvpResult result, const TrainerAward &award, bool badgeEligible) {
  return result == PVP_WIN && award.applied && badgeEligible;
}
