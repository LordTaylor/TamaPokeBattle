// Badge ring: one entry per rival, recency order, hostile blob sanitizing, award rule.
#include "pvp_badges.h"
#include "pvp_test_harness.h"

static void record(PvpBadge *ring, uint32_t id, const char *nick, uint32_t epoch, uint32_t seq) {
  pvpBadgeRecord(ring, id, nick, strlen(nick), 7, epoch, seq);
}

static void testRingDeduplicatesAndEvictsOldest() {
  PvpBadge ring[PVP_BADGE_SLOTS];
  record(ring, 1, "ONE", 100, 1);
  record(ring, 2, "TWO", 200, 2);
  EXPECT_EQ(ring[0].deviceId, 2u);
  record(ring, 1, "ONE2", 300, 3);  // known rival: wins++, moved to the front, nick refreshed
  EXPECT_EQ(ring[0].deviceId, 1u);
  EXPECT_EQ(ring[0].wins, 2);
  EXPECT_EQ(ring[0].firstEpoch, 100u);
  EXPECT_EQ(ring[0].lastEpoch, 300u);
  EXPECT_TRUE(strcmp(ring[0].nick, "ONE2") == 0);
  EXPECT_EQ(ring[1].deviceId, 2u);
  EXPECT_EQ(ring[2].deviceId, 0u);

  for (uint32_t id = 10; id < 10 + PVP_BADGE_SLOTS; id++) record(ring, id, "X", 1000 + id, 100 + id);
  EXPECT_EQ(ring[0].deviceId, 10u + PVP_BADGE_SLOTS - 1);
  int found = 0;
  for (const PvpBadge &b : ring) found += (b.deviceId == 1 || b.deviceId == 2);
  EXPECT_EQ(found, 0);  // the 9th rival pushed the oldest out
  record(ring, 0, "ZERO", 1, 1);  // deviceId 0 is not a rival
  for (const PvpBadge &b : ring) EXPECT_TRUE(b.deviceId != 0);
}

static void testSeqStaysMonotonicEvenWithStaleCaller() {
  PvpBadge ring[PVP_BADGE_SLOTS];
  record(ring, 1, "A", 1, 50);
  record(ring, 2, "B", 2, 3);  // caller's counter fell behind (lost NVS key)
  EXPECT_TRUE(ring[0].seq > ring[1].seq);
  EXPECT_EQ(ring[0].deviceId, 2u);
}

static void testSeqAtUintMaxDoesNotWrapToEmpty() {
  PvpBadge ring[PVP_BADGE_SLOTS];
  record(ring, 1, "A", 1, 0xFFFFFFFFu);
  record(ring, 2, "B", 2, 5);
  EXPECT_TRUE(ring[0].seq != 0 && ring[1].seq != 0);
  EXPECT_EQ(ring[0].deviceId, 2u);
  EXPECT_EQ(ring[1].deviceId, 1u);
  uint8_t blob[PVP_BADGE_BLOB_BYTES];
  memset(blob, 0xFF, sizeof(blob));  // hostile blob: every seq 0xFFFFFFFF, deviceId 0xFFFFFFFF (duplicates)
  PvpBadge out[PVP_BADGE_SLOTS];
  pvpBadgesDeserialize(blob, sizeof(blob), out);
  EXPECT_EQ(out[0].seq, 1u);
  EXPECT_EQ(out[1].seq, 0u);
  record(out, 9, "N", 1, 1);
  EXPECT_EQ(out[0].deviceId, 9u);
  EXPECT_EQ(out[1].deviceId, 0xFFFFFFFFu);
}

static void testHostileBlobIsSanitized() {
  PvpBadge ring[PVP_BADGE_SLOTS];
  memset(ring, 0, sizeof(ring));
  ring[0].deviceId = 5; ring[0].seq = 4; ring[0].wins = 2; ring[0].firstEpoch = 50; ring[0].lastEpoch = 60;
  memset(ring[0].nick, 'Z', sizeof(ring[0].nick));        // no terminator
  ring[1].deviceId = 6; ring[1].seq = 0;                   // seq 0 = empty
  ring[2].deviceId = 0; ring[2].seq = 9;                   // no rival
  ring[3].deviceId = 5; ring[3].seq = 8; ring[3].wins = 3; ring[3].firstEpoch = 10; ring[3].lastEpoch = 99;  // duplicate
  strcpy(ring[3].nick, "NEW");
  ring[4].deviceId = 7; ring[4].seq = 6; ring[4].nick[0] = 1; ring[4].nick[1] = (char)0xC3;  // control / non-ASCII
  uint8_t blob[PVP_BADGE_BLOB_BYTES];
  pvpBadgesSerialize(ring, blob);
  PvpBadge out[PVP_BADGE_SLOTS];
  pvpBadgesDeserialize(blob, sizeof(blob), out);

  EXPECT_EQ(out[0].deviceId, 5u);                // merged duplicate, newest seq first
  EXPECT_EQ(out[0].seq, 2u);  // renumbered 1..count
  EXPECT_EQ(out[0].wins, 5);
  EXPECT_EQ(out[0].firstEpoch, 10u);
  EXPECT_EQ(out[0].lastEpoch, 99u);
  EXPECT_TRUE(strcmp(out[0].nick, "NEW") == 0);
  EXPECT_EQ(out[1].deviceId, 7u);
  EXPECT_TRUE(out[1].nick[0] == '?' && out[1].nick[1] == '?' && out[1].nick[2] == 0);
  EXPECT_EQ(out[2].deviceId, 0u);                // compacted
  for (const PvpBadge &b : out) EXPECT_EQ(b.nick[PVP_NICK_MAX], 0);
}

static void testSerializationRoundTripAndWrongLength() {
  PvpBadge ring[PVP_BADGE_SLOTS];
  record(ring, 0xAABBCCDDu, "ROUND", 123456, 7);
  uint8_t blob[PVP_BADGE_BLOB_BYTES];
  pvpBadgesSerialize(ring, blob);
  PvpBadge out[PVP_BADGE_SLOTS];
  pvpBadgesDeserialize(blob, sizeof(blob), out);
  EXPECT_EQ(out[0].deviceId, 0xAABBCCDDu);
  EXPECT_EQ(out[0].firstEpoch, 123456u);
  EXPECT_EQ(out[0].oppLevel, 7);
  EXPECT_TRUE(strcmp(out[0].nick, "ROUND") == 0);
  pvpBadgesDeserialize(blob, sizeof(blob) - 1, out);
  EXPECT_EQ(out[0].deviceId, 0u);                // wrong length = empty ring
  pvpBadgesDeserialize(blob, sizeof(blob) + 1, out);
  EXPECT_EQ(out[0].deviceId, 0u);
}

static void testBadgeAwardRule() {
  TrainerAward applied;
  applied.applied = true;
  TrainerAward blocked;
  blocked.blocked = true;
  EXPECT_TRUE(pvpBadgeEarned(PVP_WIN, applied, true));
  EXPECT_TRUE(!pvpBadgeEarned(PVP_WIN, applied, false));   // walkover
  EXPECT_TRUE(!pvpBadgeEarned(PVP_WIN, blocked, true));    // FarmGuard / no clock
  EXPECT_TRUE(!pvpBadgeEarned(PVP_LOSS, applied, true));
  EXPECT_TRUE(!pvpBadgeEarned(PVP_DRAW, applied, true));
  EXPECT_TRUE(!pvpBadgeEarned(PVP_ABORTED, applied, true));
}

int main() {
  testRingDeduplicatesAndEvictsOldest();
  testSeqStaysMonotonicEvenWithStaleCaller();
  testSeqAtUintMaxDoesNotWrapToEmpty();
  testHostileBlobIsSanitized();
  testSerializationRoundTripAndWrongLength();
  testBadgeAwardRule();
  if (failures) {
    std::cerr << failures << " Fehler\n";
    return 1;
  }
  std::cout << "pvp_badges_tests: OK\n";
  return 0;
}
