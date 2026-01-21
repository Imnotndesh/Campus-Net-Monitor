#include <Arduino.h>
#include "storage/StorageManager.h"
#include "connection/ConnectionManager.h"
#include "diagnostics/DiagnosticEngine.h"

// Helper to convert the rating enum to a string for the logs
String getRatingString(CongestionRating r) {
    switch(r) {
        case GOOD: return "GOOD";
        case BAD: return "BAD";
        case TERRIBLE: return "TERRIBLE";
        default: return "UNKNOWN";
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); } // Wait for terminal
    delay(2000);

    Serial.println("\n--- Starting Diagnostic Verification Test ---");

    // 1. Initialize Modules
    StorageManager::begin();
    ConnectionManager::begin();

    // 2. Load Stored Credentials
    WifiCredentials creds = StorageManager::loadWifiCredentials();
    
    if (creds.ssid == "") {
        Serial.println("[ERROR] No credentials found in storage. Run Captive Portal first.");
        return;
    }

    // 3. Attempt Connection
    Serial.printf("Attempting to connect to: %s\n", creds.ssid.c_str());
    if (ConnectionManager::tryConnect(creds.ssid, creds.password)) {
        
        // --- TEST 1: LIGHT DIAGNOSTICS ---
        Serial.println("\n[1/2] RUNNING LIGHT DIAGNOSTICS...");
        NetworkMetrics lightResults = DiagnosticEngine::performFullTest("8.8.8.8");
        
        Serial.println("---------- LIGHT RESULTS ----------");
        Serial.printf("RSSI: %d dBm | BSSID: %s | Channel: %d\n", 
                      lightResults.rssi, lightResults.bssid.c_str(), lightResults.channel);
        Serial.printf("Latency: %d ms | DNS: %d ms | Loss: %.1f%%\n", 
                      lightResults.avgLatency, lightResults.dnsResolutionTime, lightResults.packetLoss);
        Serial.printf("Congestion: %s (%d neighbors)\n", 
                      getRatingString(lightResults.congestion).c_str(), lightResults.neighborCount);
        Serial.println("-----------------------------------\n");

        delay(2000); // Brief pause between tests

        // --- TEST 2: ENHANCED (DEEP) DIAGNOSTICS ---
        Serial.println("[2/2] RUNNING ENHANCED (DEEP) DIAGNOSTICS...");
        // Note: This may involve temporary disconnection for radio analysis
        EnhancedMetrics deepResults = DiagnosticEngine::performDeepAnalysis("8.8.8.8");

        Serial.println("---------- DEEP RESULTS ----------");
        Serial.printf("Link Quality Score: %.1f / 100\n", deepResults.linkQuality);
        Serial.printf("SNR: %.1f dB | Noise Floor: %d dBm\n", deepResults.snr, deepResults.noiseFloor);
        Serial.printf("PHY Mode: %s | Estimated Throughput: %d kbps\n", 
                      deepResults.phyMode.c_str(), deepResults.tcpThroughput);
        Serial.printf("Channel Utilization: %.1f%%\n", deepResults.channelUtilization);
        Serial.printf("Probe Uptime: %u seconds\n", deepResults.uptime);
        Serial.println("----------------------------------");

    } else {
        Serial.println("[ERROR] Connection failed. Check AP availability.");
    }

    Serial.println("\n--- Test Sequence Complete ---");
}

void loop() {
    // Stay idle after test
    delay(1000);
}