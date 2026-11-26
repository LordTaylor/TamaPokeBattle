# PvP protocol (T3): scope and known gaps

Pure logic in `pvp_*.h/.cpp`; transport (ESP-NOW) and UI live elsewhere. Contract for the
integrator is in the header comment of `pvp_session.h`.

## Wire format

Little-endian, fixed sizes, at most `PVP_MAX_PACKET` (48) bytes, far below the ESP-NOW limit of 250.
Header (13 bytes): magic `0x5450`, proto, type, session, seq, payload length, CRC-16/CCITT-FALSE
over the whole packet with the CRC field zeroed. Protocol version is 2. `deviceId` is the last
four MAC bytes (`pvpDeviceIdFromMac`); HELLO/CHALLENGE/ACCEPT with a mismatching id are dropped.

## Identity contract

The id is `pvpDeviceIdFromMac(MAC)` with the MAC from `esp_wifi_get_mac(WIFI_IF_STA)` (WiFi in
`WIFI_STA` mode), and ESP-NOW peers use `ifidx = WIFI_IF_STA`. `(uint32_t)ESP.getEfuseMac()` has
the reverse byte order and would make every packet get rejected. Honest scope: binding the id
to the MAC protects the peer table against id spoofing; it does not stop someone running a
modified firmware from changing its MAC (`esp_wifi_set_mac`) to farm points against itself.

## Outcome rules

- Only `FINISHED` counts (points, badge). `ABORTED` (timeout, desync, user abort) counts for nobody.
- Both sides agree on FINISHED via the final `ROUND_HASH` (flag `final`) or, when that hash was lost,
  via the peer's BYE. After FINISHED the session keeps retransmitting for `PVP_LINGER_MS`;
  `canLeave()` tells the UI when it is safe to leave.

## Deliberately deferred (decision: owner, not forgotten)

- **No commit-reveal.** Nonces and actions are exchanged in clear. The acceptor sees the
  challenger's nonce before choosing its own, and a modified firmware can see the peer's action
  before choosing its own. Fair play therefore assumes unmodified firmware.
- **No encryption or authentication.** MAC addresses can be spoofed; CRC protects against
  corruption only. Threat model: someone with a modified firmware can write points to their own
  NVS anyway, so PvP cheating hurts an honest player only marginally (a loss gives 1 point
  instead of 3).
- **Stats are validated by range only** (level 1..100, atk/def/spe 1..999, hp 0..999 where 0 means
  "receiver derives it", types 0..18). Consistency with the Pokedex (stats per level, types per
  species) must be checked by the engine-side validator before use.
- **Quitting loser:** an ABORT gives 0 points to both sides, including the winner. Options:
  keep as is (simple, nobody profits from disconnecting), or award the opponent a win when the
  peer aborts after the last confirmed round with an advantage (needs the engine state, not
  protocol-level; not implemented).
