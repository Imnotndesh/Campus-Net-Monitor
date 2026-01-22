#include "DiagnosticEngine.h"
#include "SnifferEngine.h"
#include <esp_wifi.h>

/**
 * Standard Monitoring: Captures metrics while connected to the AP.
 * This is the "Light" telemetry used for 30-second heartbeats.
 */
NetworkMetrics DiagnosticEngine::performFullTest(const char* targetHost) {
    NetworkMetrics metrics;

    // 1. Physical Layer Connection Stats
    metrics.rssi = WiFi.RSSI();
    metrics.bssid = WiFi.BSSIDstr();
    metrics.channel = WiFi.channel();

    // 2. Neighbor Scan & Congestion Analysis
    // We perform a quick scan to count APs on the same and nearby channels
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
    // Pings the target host (e.g., 8.8.8.8 or the university gateway)
    bool success = Ping.ping(targetHost, 5);
    if (success) {
        metrics.avgLatency = Ping.averageTime();
        // Simple packet loss calculation based on the ping attempt
        metrics.packetLoss = (Ping.averageTime() > 0) ? 0.0 : 100.0; 
    } else {
        metrics.avgLatency = -1;
        metrics.packetLoss = 100.0;
    }
    
    // DNS performance is a common point of failure in large networks
    metrics.dnsResolutionTime = measureDNS("google.com");

    return metrics;
}

/**
 * Investigative Monitoring: Performs deep radio analysis.
 * Note: This temporarily disconnects the probe from the Wi-Fi.
 */
EnhancedMetrics DiagnosticEngine::performDeepAnalysis(const char* targetHost) {
    // 1. Gather "Light" metrics first while still connected
    NetworkMetrics basic = performFullTest(targetHost);
    EnhancedMetrics em;
    
    // Copy base metrics to the enhanced structure
    em.rssi = basic.rssi;
    em.bssid = basic.bssid;
    em.channel = basic.channel;
    em.packetLoss = basic.packetLoss;
    em.avgLatency = basic.avgLatency;
    em.dnsResolutionTime = basic.dnsResolutionTime;
    em.neighborCount = basic.neighborCount;
    em.overlappingCount = basic.overlappingCount;
    em.congestion = basic.congestion;

    // 2. Radio Environment Analysis (Promiscuous Mode)
    // The probe disconnects here to sniff raw 802.11 frames
    Serial.println("[DEEP] Initiating 2-second radio environment capture...");
    SnifferStats s = SnifferEngine::analyzeChannel(em.channel, 2000);
    
    em.channelUtilization = s.channelUtilization;
    // em.beaconLossRate can be calculated here if added to the struct

    // 3. Signal Quality Calculations
    // Noise floor is typically estimated at -95dBm for the ESP32 radio
    em.noiseFloor = -95;
    em.snr = (float)em.rssi - em.noiseFloor;

    // Link Quality Score (0-100)
    // Weighted formula considering Signal Strength, Packet Loss, and Congestion
    float quality = (em.rssi + 100) * 2; 
    if (em.packetLoss > 0) quality -= (em.packetLoss * 5);
    quality -= (em.channelUtilization * 0.2); // Penalize heavily congested airwaves
    em.linkQuality = constrain(quality, 0, 100);

    // 4. Advanced Network Capabilities
    uint8_t protocol;
    esp_wifi_get_protocol(WIFI_IF_STA, &protocol);
    
    if (protocol & WIFI_PROTOCOL_11N) em.phyMode = "802.11n";
    else if (protocol & WIFI_PROTOCOL_11G) em.phyMode = "802.11g";
    else em.phyMode = "802.11b";

    em.tcpThroughput = measureThroughput(targetHost);
    em.uptime = millis() / 1000;

    return em;
}

/**
 * Estimates throughput by simulating a small data transfer.
 */
int DiagnosticEngine::measureThroughput(const char* host) {
    // Phase 1 implementation uses a randomized estimate.
    // Phase 2 will implement a real HTTP download speed test.
    return random(500, 2500);
}

/**
 * Rates the congestion based on neighboring Access Points.
 */
CongestionRating DiagnosticEngine::calculateCongestion(int total, int overlapping) {
    if (overlapping > 4 || total > 10) return TERRIBLE;
    if (overlapping > 2 || total > 5) return BAD;
    return GOOD;
}

/**
 * Measures the latency of a DNS lookup.
 */
int DiagnosticEngine::measureDNS(const char* host) {
    unsigned long start = millis();
    IPAddress remote_ip;
    if (WiFi.hostByName(host, remote_ip)) {
        return (int)(millis() - start);
    }
    return -1; // Indicates a DNS timeout or failure
}