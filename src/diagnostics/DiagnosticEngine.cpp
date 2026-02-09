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
    Serial.println("[DEEP] Initiating 2-second radio environment capture...");
    SnifferStats s = SnifferEngine::analyzeChannel(em.channel, 2000);
    
    em.channelUtilization = s.channelUtilization;
    em.noiseFloor = -95;
    em.snr = (float)em.rssi - em.noiseFloor;

    // Link Quality Score (0-100)
    float quality = (em.rssi + 100) * 2; 
    if (em.packetLoss > 0) quality -= (em.packetLoss * 5);
    quality -= (em.channelUtilization * 0.2); // Penalize heavily congested airwaves
    em.linkQuality = constrain(quality, 0, 100);
    uint8_t protocol;
    esp_wifi_get_protocol(WIFI_IF_STA, &protocol);
    
    if (protocol & WIFI_PROTOCOL_11N) em.phyMode = "802.11n";
    else if (protocol & WIFI_PROTOCOL_11G) em.phyMode = "802.11g";
    else em.phyMode = "802.11b";

    em.tcpThroughput = measureThroughput(targetHost);
    em.uptime = millis() / 1000;

    return em;
}

int DiagnosticEngine::measureThroughput(const char* host) {
    Serial.println("[DIAG] Measuring TCP throughput...");
    
    const int NUM_ATTEMPTS = 3;
    int results[NUM_ATTEMPTS];
    int validResults = 0;
    
    for (int attempt = 0; attempt < NUM_ATTEMPTS; attempt++) {
        WiFiClient client;
        const int port = 80;
        const int testDataSize = 8192; // 8KB test
        
        Serial.printf("[DIAG] Attempt %d/%d...\n", attempt + 1, NUM_ATTEMPTS);
        
        // Try to connect
        if (!client.connect(host, port, 3000)) { // 3 second timeout
            Serial.println("[DIAG] Connection failed");
            results[attempt] = -1;
            continue;
        }
        
        // Send HTTP GET request
        String request = "GET / HTTP/1.1\r\n";
        request += "Host: ";
        request += host;
        request += "\r\n";
        request += "Connection: close\r\n\r\n";
        
        unsigned long startTime = millis();
        client.print(request);
        
        // Wait for response headers
        while (client.connected() && !client.available()) {
            if (millis() - startTime > 3000) break;
            delay(1);
        }
        
        // Skip headers to get to body
        bool headersComplete = false;
        unsigned long headerTimeout = millis();
        while (client.connected() || client.available()) {
            if (client.available()) {
                String line = client.readStringUntil('\n');
                if (line == "\r" || line == "") {
                    headersComplete = true;
                    break;
                }
                headerTimeout = millis();
            }
            if (millis() - headerTimeout > 2000) break;
        }
        
        if (!headersComplete) {
            Serial.println("[DIAG] Failed to read headers");
            client.stop();
            results[attempt] = -1;
            continue;
        }
        
        // Now measure body download speed
        unsigned long dataStartTime = millis();
        int totalBytes = 0;
        unsigned long lastByteTime = millis();
        
        while (client.connected() || client.available()) {
            if (client.available()) {
                char c = client.read();
                totalBytes++;
                lastByteTime = millis();
            }
            
            // Stop conditions
            if (totalBytes >= testDataSize) break;
            if (millis() - lastByteTime > 2000) break; // 2s idle timeout
        }
        
        unsigned long dataEndTime = millis();
        client.stop();
        
        if (totalBytes == 0) {
            Serial.println("[DIAG] No data received");
            results[attempt] = -1;
            continue;
        }
        
        // Calculate throughput
        float durationSec = (dataEndTime - dataStartTime) / 1000.0;
        if (durationSec <= 0) durationSec = 0.001;
        
        float bytesPerSec = totalBytes / durationSec;
        int kbps = (int)((bytesPerSec * 8) / 1024);
        
        Serial.printf("[DIAG] Attempt %d: %d bytes in %.2fs = %d kbps\n", 
                      attempt + 1, totalBytes, durationSec, kbps);
        
        results[attempt] = kbps;
        validResults++;
        
        delay(100); // Small delay between attempts
    }
    
    // Calculate average of valid results
    if (validResults == 0) {
        Serial.println("[DIAG] All throughput attempts failed");
        return 0; // Return 0 instead of -1 for failed test
    }
    
    int sum = 0;
    int count = 0;
    for (int i = 0; i < NUM_ATTEMPTS; i++) {
        if (results[i] > 0) {
            sum += results[i];
            count++;
        }
    }
    
    int avgKbps = count > 0 ? sum / count : 0;
    Serial.printf("[DIAG] Average throughput: %d kbps (%d/%d valid attempts)\n", 
                  avgKbps, validResults, NUM_ATTEMPTS);
    
    return avgKbps;
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