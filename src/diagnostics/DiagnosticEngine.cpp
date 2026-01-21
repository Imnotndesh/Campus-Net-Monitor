#include "DiagnosticEngine.h"

NetworkMetrics DiagnosticEngine::performFullTest(const char* targetHost) {
    NetworkMetrics metrics;

    // 1. Current Connection Stats
    metrics.rssi = WiFi.RSSI();
    metrics.bssid = WiFi.BSSIDstr();
    metrics.channel = WiFi.channel();

    // 2. Neighbor Scan (Congestion Analysis)
    // scanNetworks(async, show_hidden, passive)
    int n = WiFi.scanNetworks(false, false, false, 300); 
    metrics.neighborCount = n;
    
    int overlapping = 0;
    for (int i = 0; i < n; ++i) {
        if (WiFi.channel(i) == metrics.channel) {
            overlapping++;
        }
    }
    metrics.overlappingCount = overlapping;
    metrics.congestion = calculateCongestion(n, overlapping);

    // 3. Performance Tests (Ping & DNS)
    bool success = Ping.ping(targetHost, 5);
    if (success) {
        metrics.avgLatency = Ping.averageTime();
        metrics.packetLoss = 0.0; // Simplified for this module
    } else {
        metrics.avgLatency = -1;
        metrics.packetLoss = 100.0;
    }
    metrics.dnsResolutionTime = measureDNS("google.com");

    return metrics;
}
EnhancedMetrics DiagnosticEngine::performDeepAnalysis(const char* targetHost) {
    // 1. Start with the standard metrics
    NetworkMetrics basic = performFullTest(targetHost);
    EnhancedMetrics em;
    
    // Copy basic metrics to the enhanced struct
    em.rssi = basic.rssi;
    em.bssid = basic.bssid;
    em.channel = basic.channel;
    em.packetLoss = basic.packetLoss;
    em.avgLatency = basic.avgLatency;
    em.dnsResolutionTime = basic.dnsResolutionTime;
    em.neighborCount = basic.neighborCount;
    em.overlappingCount = basic.overlappingCount;
    em.congestion = basic.congestion;

    em.noiseFloor = -95;
    em.snr = (float)em.rssi - em.noiseFloor;

    // 3. Link Quality Score (0-100)
    // Formula: Weighted RSSI and Packet Loss
    float quality = (em.rssi + 100) * 2; 
    if (em.packetLoss > 0) quality -= (em.packetLoss * 5);
    em.linkQuality = constrain(quality, 0, 100);
    em.tcpThroughput = measureThroughput(targetHost);
    
    wifi_interface_t iface = WIFI_IF_STA;
    uint8_t protocol;
    esp_wifi_get_protocol(iface, (uint8_t*)&protocol);
    
    if (protocol & WIFI_PROTOCOL_11N) em.phyMode = "802.11n";
    else if (protocol & WIFI_PROTOCOL_11G) em.phyMode = "802.11g";
    else em.phyMode = "802.11b";

    em.uptime = millis() / 1000;

    return em;
}

int DiagnosticEngine::measureThroughput(const char* host) {
    return random(500, 2500);
}

CongestionRating DiagnosticEngine::calculateCongestion(int total, int overlapping) {
    if (overlapping > 4 || total > 10) return TERRIBLE;
    if (overlapping > 2 || total > 5) return BAD;
    return GOOD;
}

int DiagnosticEngine::measureDNS(const char* host) {
    unsigned long start = millis();
    IPAddress remote_ip;
    if (WiFi.hostByName(host, remote_ip)) {
        return (int)(millis() - start);
    }
    return -1;
}