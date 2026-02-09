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
        metrics.packetLoss = (Ping.averageTime() > 0) ? 0.0 : 100.0; 
    } else {
        metrics.avgLatency = -1;
        metrics.packetLoss = 100.0;
    }
    
    metrics.dnsResolutionTime = measureDNS("google.com");

    return metrics;
}

/**
 * FIXED: Investigative Monitoring with proper operation ordering.
 * 
 * Operation Order:
 * 1. Gather basic metrics (while connected)
 * 2. Measure TCP throughput (requires connection)
 * 3. Enter promiscuous mode for radio analysis (disconnects WiFi)
 * 4. Calculate final metrics
 * 
 * NOTE: WiFi will be DISCONNECTED after this function completes.
 * The caller MUST handle reconnection.
 */
EnhancedMetrics DiagnosticEngine::performDeepAnalysis(const char* targetHost) {
    Serial.println("\n[DEEP] ╔══════════════════════════════════════");
    Serial.println("[DEEP] ║ DEEP SCAN INITIATED");
    Serial.println("[DEEP] ╠══════════════════════════════════════");
    
    // PHASE 1: Gather metrics while connected
    Serial.println("[DEEP] ║ Phase 1: Gathering basic metrics...");
    NetworkMetrics basic = performFullTest(targetHost);
    EnhancedMetrics em;
    
    // Copy base metrics to enhanced structure
    em.rssi = basic.rssi;
    em.bssid = basic.bssid;
    em.channel = basic.channel;
    em.packetLoss = basic.packetLoss;
    em.avgLatency = basic.avgLatency;
    em.dnsResolutionTime = basic.dnsResolutionTime;
    em.neighborCount = basic.neighborCount;
    em.overlappingCount = basic.overlappingCount;
    em.congestion = basic.congestion;
    
    Serial.printf("[DEEP] ║   ✓ RSSI: %d dBm\n", em.rssi);
    Serial.printf("[DEEP] ║   ✓ Channel: %d\n", em.channel);
    Serial.printf("[DEEP] ║   ✓ Neighbors: %d (Overlapping: %d)\n", 
                  em.neighborCount, em.overlappingCount);
    
    // PHASE 2: Measure TCP throughput (BEFORE promiscuous mode)
    Serial.println("[DEEP] ║ Phase 2: Measuring TCP throughput...");
    em.tcpThroughput = measureThroughput(targetHost);
    Serial.printf("[DEEP] ║   ✓ Throughput: %d kbps\n", em.tcpThroughput);
    
    // Get PHY mode while still connected
    uint8_t protocol;
    esp_wifi_get_protocol(WIFI_IF_STA, &protocol);
    if (protocol & WIFI_PROTOCOL_11N) em.phyMode = "802.11n";
    else if (protocol & WIFI_PROTOCOL_11G) em.phyMode = "802.11g";
    else em.phyMode = "802.11b";
    
    em.uptime = millis() / 1000;
    
    // PHASE 3: Radio environment analysis (will disconnect WiFi)
    Serial.println("[DEEP] ║ Phase 3: Radio environment capture...");
    Serial.println("[DEEP] ║   ⚠ WARNING: Entering promiscuous mode");
    Serial.println("[DEEP] ║   ⚠ WiFi will disconnect temporarily");
    
    SnifferStats s = SnifferEngine::analyzeChannel(em.channel, 2000);
    
    em.channelUtilization = s.channelUtilization;
    Serial.printf("[DEEP] ║   ✓ Channel utilization: %.1f%%\n", em.channelUtilization);
    Serial.printf("[DEEP] ║   ✓ Beacons: %d (Missed: %d)\n", 
                  s.beaconCount, s.missedBeacons);
    Serial.printf("[DEEP] ║   ✓ Data packets: %d\n", s.dataPackets);
    
    // PHASE 4: Calculate derived metrics
    Serial.println("[DEEP] ║ Phase 4: Calculating quality scores...");
    em.noiseFloor = -95;
    em.snr = (float)em.rssi - em.noiseFloor;
    
    // Link Quality Score (0-100)
    float quality = (em.rssi + 100) * 2; 
    if (em.packetLoss > 0) quality -= (em.packetLoss * 5);
    quality -= (em.channelUtilization * 0.2);
    em.linkQuality = constrain(quality, 0, 100);
    
    Serial.printf("[DEEP] ║   ✓ SNR: %.1f dB\n", em.snr);
    Serial.printf("[DEEP] ║   ✓ Link Quality: %.1f/100\n", em.linkQuality);
    Serial.println("[DEEP] ╠══════════════════════════════════════");
    Serial.println("[DEEP] ║ DEEP SCAN COMPLETED");
    Serial.println("[DEEP] ║ ⚠ WiFi is DISCONNECTED");
    Serial.println("[DEEP] ║ ⚠ Caller must reconnect");
    Serial.println("[DEEP] ╚══════════════════════════════════════\n");

    return em;
}

int DiagnosticEngine::measureThroughput(const char* host) {
    Serial.println("[DIAG] Measuring TCP throughput...");
    
    // Pre-check: Ensure WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[DIAG] ❌ WiFi not connected, skipping throughput test");
        return 0;
    }
    
    const int NUM_ATTEMPTS = 3;
    int results[NUM_ATTEMPTS];
    int validResults = 0;
    
    for (int attempt = 0; attempt < NUM_ATTEMPTS; attempt++) {
        // Double-check connection before each attempt
        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("[DIAG] Attempt %d/%d: WiFi disconnected, aborting\n", 
                          attempt + 1, NUM_ATTEMPTS);
            results[attempt] = -1;
            break;
        }
        
        WiFiClient client;
        const int port = 80;
        const int testDataSize = 8192; // 8KB test
        
        Serial.printf("[DIAG] Attempt %d/%d...\n", attempt + 1, NUM_ATTEMPTS);
        
        // Try to connect
        if (!client.connect(host, port, 3000)) {
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
        
        // Measure body download speed
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
            if (millis() - lastByteTime > 2000) break;
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
        
        delay(100);
    }
    
    // Calculate average of valid results
    if (validResults == 0) {
        Serial.println("[DIAG] All throughput attempts failed");
        return 0;
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
    Serial.printf("[DIAG] ✓ Average throughput: %d kbps (%d/%d valid)\n", 
                  avgKbps, validResults, NUM_ATTEMPTS);
    
    return avgKbps;
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