#include "SnifferEngine.h"
#include <WiFi.h>

// Static variables to hold data during the asynchronous callback
static volatile int totalPackets = 0;
static volatile int beaconPackets = 0;
static volatile uint32_t lastSeq = 0;
static volatile int seqGaps = 0;

void SnifferEngine::snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    totalPackets++;
    
    // We only care about Management Frames (Beacons)
    if (type == WIFI_PKT_MGMT) {
        wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
        uint8_t *payload = pkt->payload;
        
        // Frame Control: Subtype 0x08 is a Beacon
        if ((payload[0] & 0xFC) == 0x80) {
            beaconPackets++;
            
            // Extract Sequence Number (2 bytes at offset 22)
            uint16_t seq = ((payload[23] << 8) | payload[22]) >> 4;
            if (lastSeq != 0 && seq > lastSeq + 1) {
                seqGaps += (seq - lastSeq - 1);
            }
            lastSeq = seq;
        }
    }
}

SnifferStats SnifferEngine::analyzeChannel(int channel, uint32_t durationMs) {
    SnifferStats stats = {0};
    
    // Reset counters
    totalPackets = 0; beaconPackets = 0; lastSeq = 0; seqGaps = 0;
    
    // Disconnect and enter Promiscuous Mode
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    delay(durationMs); // Sniffing window

    // Exit Promiscuous Mode
    esp_wifi_set_promiscuous(false);
    
    // Calculate results
    stats.beaconCount = beaconPackets;
    stats.missedBeacons = seqGaps;
    stats.dataPackets = totalPackets - beaconPackets;
    
    // utilization estimation: packet density over time
    stats.channelUtilization = (totalPackets / (float)durationMs) * 100.0;
    stats.channelUtilization = constrain(stats.channelUtilization, 0.0, 100.0);

    return stats;
}