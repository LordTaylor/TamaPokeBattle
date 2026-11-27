#pragma once

#include <stddef.h>
#include <stdint.h>

#include "trainer.h"

// One badge = one rival (key: deviceId taken from the sender MAC).
struct PvpBadge {
  uint32_t deviceId = 0;    // 0 = empty
  uint32_t firstEpoch = 0;  // first settled win
  uint32_t lastEpoch = 0;   // last settled win
  uint32_t seq = 0;         // order, newest = largest; 0 = empty
  uint16_t wins = 0;
  uint8_t oppLevel = 0;     // rival level at the last win
  char nick[PVP_NICK_MAX + 1] = "";
};

constexpr uint8_t PVP_BADGE_SLOTS = 8;
constexpr uint8_t PVP_BADGE_WIRE_BYTES = 4 * 4 + 2 + 1 + PVP_NICK_MAX + 1;  // field by field, no padding
constexpr size_t PVP_BADGE_BLOB_BYTES = (size_t)PVP_BADGE_SLOTS * PVP_BADGE_WIRE_BYTES;
constexpr char PVP_BADGE_KEY[] = "tbadge";  // NVS key (<= 15 chars), outside PetBackup

// Newest at [0]. Known deviceId: wins++ and moved to the front; otherwise inserted,
// the oldest drops out. `seq` is raised above every existing seq so ordering holds
// (sanitize renumbers seq to 1..count, so it never wraps).
void pvpBadgeRecord(PvpBadge *ring, uint32_t deviceId, const char *nick, size_t nickLen,
                    uint8_t oppLevel, uint32_t nowEpoch, uint32_t seq);

// After loading: terminates/cleans nicks, drops empty entries (deviceId or seq == 0),
// merges duplicate deviceIds, sorts newest first, compacts.
void sanitizePvpBadges(PvpBadge *ring);

// Blob <-> ring. Deserialize treats a wrong length as an empty ring, then sanitizes.
void pvpBadgesSerialize(const PvpBadge *ring, uint8_t *out);
void pvpBadgesDeserialize(const uint8_t *blob, size_t len, PvpBadge *ring);

// A badge only for a settled win (points applied) against a rival that counts.
bool pvpBadgeEarned(PvpResult result, const TrainerAward &award, bool badgeEligible);
