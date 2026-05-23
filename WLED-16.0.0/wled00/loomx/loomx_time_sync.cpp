#include "loomx_time_sync.h"
#include "../wled.h"

namespace Loomx { namespace TimeSync {

  static constexpr uint16_t LOOMX_TIME_PORT = 42716;
  static constexpr uint32_t LOOMX_TIME_INTERVAL_MS = 250;
  static constexpr uint8_t LOOMX_TIME_PACKET_SIZE = 9;
  static constexpr uint8_t LOOMX_TIME_VERSION = 1;

  static WiFiUDP loomxTimeUdp;
  static bool udpStarted = false;
  static bool synced = false;
  static uint32_t lastBroadcastMs = 0;
  static int32_t parentToLocalOffsetMs = 0;

  static void ensureUdp()
  {
    if (udpStarted) return;
    udpStarted = loomxTimeUdp.begin(LOOMX_TIME_PORT);
  }

  static void writeU32(uint8_t* out, uint32_t value)
  {
    out[0] = (value >> 24) & 0xFF;
    out[1] = (value >> 16) & 0xFF;
    out[2] = (value >> 8) & 0xFF;
    out[3] = value & 0xFF;
  }

  static uint32_t readU32(const uint8_t* in)
  {
    return (uint32_t(in[0]) << 24) | (uint32_t(in[1]) << 16) | (uint32_t(in[2]) << 8) | uint32_t(in[3]);
  }

  void parentBroadcast()
  {
    ensureUdp();
    if (!udpStarted) return;

    const uint32_t now = millis();
    if (lastBroadcastMs != 0 && now - lastBroadcastMs < LOOMX_TIME_INTERVAL_MS) return;
    lastBroadcastMs = now;

    uint8_t packet[LOOMX_TIME_PACKET_SIZE] = { 'L', 'M', 'X', 'T', LOOMX_TIME_VERSION, 0, 0, 0, 0 };
    writeU32(packet + 5, now);
    loomxTimeUdp.beginPacket(IPAddress(255, 255, 255, 255), LOOMX_TIME_PORT);
    loomxTimeUdp.write(packet, sizeof(packet));
    loomxTimeUdp.endPacket();
  }

  void childPoll()
  {
    ensureUdp();
    if (!udpStarted) return;

    int packetSize = loomxTimeUdp.parsePacket();
    while (packetSize > 0) {
      uint8_t packet[LOOMX_TIME_PACKET_SIZE];
      const int len = loomxTimeUdp.read(packet, sizeof(packet));
      if (len == LOOMX_TIME_PACKET_SIZE &&
          packet[0] == 'L' && packet[1] == 'M' && packet[2] == 'X' && packet[3] == 'T' &&
          packet[4] == LOOMX_TIME_VERSION) {
        childIngest(readU32(packet + 5), millis());
      }
      packetSize = loomxTimeUdp.parsePacket();
    }
  }

  void childIngest(uint32_t parentMillis, uint32_t childArrivalMillis)
  {
    const int32_t sampleOffset = int32_t(childArrivalMillis - parentMillis);
    if (!synced) {
      parentToLocalOffsetMs = sampleOffset;
      synced = true;
      return;
    }

    const int32_t delta = sampleOffset - parentToLocalOffsetMs;
    if (delta > 250 || delta < -250) parentToLocalOffsetMs = sampleOffset;
    else parentToLocalOffsetMs += delta / 8;
  }

  uint32_t localFor(uint32_t parentMillis)
  {
    return parentMillis + uint32_t(parentToLocalOffsetMs);
  }

  bool isSynced()
  {
    return synced;
  }

  void reset()
  {
    synced = false;
    parentToLocalOffsetMs = 0;
  }

}}
