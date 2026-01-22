#ifndef SNIFFER_ENGINE_H
#define SNIFFER_ENGINE_H

#include <Arduino.h>
#include <esp_wifi.h>

struct SnifferStats {
    float channelUtilization; // 0-100%
    int beaconCount;
    int missedBeacons;
    int dataPackets;
};

class SnifferEngine {
public:
    static SnifferStats analyzeChannel(int channel, uint32_t durationMs);
private:
    static void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type);
};

#endif