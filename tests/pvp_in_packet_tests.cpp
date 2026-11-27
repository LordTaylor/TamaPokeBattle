#include "pvp_test_harness.h"
#include "../pvp_in_packet.h"

static const uint8_t SRC[6] = {0x34, 0x85, 0x18, 0xAA, 0xBB, 0xCC};

// Real wire packet from the serializer: the filter must accept what the protocol sends.
static void testAcceptsSerializedPacket() {
  PvpMessage m;
  m.type = PVP_MSG_HELLO;
  m.deviceId = pvpDeviceIdFromMac(SRC);
  strcpy(m.nick, "ASH");
  m.trainerLevel = 3;
  uint8_t buf[PVP_MAX_PACKET];
  size_t n = pvpSerialize(m, buf, sizeof(buf));
  EXPECT_TRUE(n > 0);
  PvpInPacket p;
  EXPECT_TRUE(pvpInPacketFrom(SRC, buf, (int)n, p));
  EXPECT_EQ((int)p.len, (int)n);
  EXPECT_TRUE(memcmp(p.mac, SRC, 6) == 0);
  EXPECT_TRUE(memcmp(p.data, buf, n) == 0);
}

static void testRejectsBadLengthAndPointers() {
  uint8_t buf[PVP_MAX_PACKET + 4] = {0};
  buf[0] = PVP_MAGIC & 0xFF;
  buf[1] = PVP_MAGIC >> 8;
  PvpInPacket p;
  EXPECT_TRUE(!pvpInPacketFrom(SRC, buf, 0, p));
  EXPECT_TRUE(!pvpInPacketFrom(SRC, buf, 1, p));
  EXPECT_TRUE(!pvpInPacketFrom(SRC, buf, -1, p));
  EXPECT_TRUE(pvpInPacketFrom(SRC, buf, 2, p));
  EXPECT_TRUE(pvpInPacketFrom(SRC, buf, PVP_MAX_PACKET, p));
  EXPECT_TRUE(!pvpInPacketFrom(SRC, buf, PVP_MAX_PACKET + 1, p));
  EXPECT_TRUE(!pvpInPacketFrom(nullptr, buf, 20, p));
  EXPECT_TRUE(!pvpInPacketFrom(SRC, nullptr, 20, p));
}

static void testRejectsForeignMagicAndKeepsOutUntouched() {
  uint8_t buf[20] = {0};
  PvpInPacket p;
  p.len = 77;
  buf[0] = PVP_MAGIC >> 8;  // swapped bytes: big-endian by mistake
  buf[1] = PVP_MAGIC & 0xFF;
  EXPECT_TRUE(!pvpInPacketFrom(SRC, buf, 20, p));
  buf[0] = PVP_MAGIC & 0xFF;
  buf[1] = 0;
  EXPECT_TRUE(!pvpInPacketFrom(SRC, buf, 20, p));
  EXPECT_EQ((int)p.len, 77);  // a rejected frame must not touch the output
}

int main() {
  testAcceptsSerializedPacket();
  testRejectsBadLengthAndPointers();
  testRejectsForeignMagicAndKeepsOutUntouched();
  if (failures) {
    std::cerr << failures << " Fehler\n";
    return 1;
  }
  std::cout << "pvp_in_packet_tests: alle bestanden\n";
  return 0;
}
