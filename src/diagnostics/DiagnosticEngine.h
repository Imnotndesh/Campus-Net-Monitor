#ifndef DIAGNOSTIC_ENGINE_H
#define DIAGNOSTIC_ENGINE_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <esp_wifi.h>

enum CongestionRating { GOOD, BAD, TERRIBLE };

struct NetworkMetrics {
    int rssi;
    String bssid;
    int channel;
    float packetLoss;
    int avgLatency;
    int dnsResolutionTime;
    int neighborCount;
    int overlappingCount;
    CongestionRating congestion;
};

struct EnhancedMetrics : NetworkMetrics {
    float snr;
    int8_t noiseFloor;
    float linkQuality;       // 0-100 score
    float channelUtilization; // Estimated %
    int tcpThroughput;        // kbps
    String phyMode;           // 802.11b/g/n
    uint32_t uptime;          // Connection uptime
};

class DiagnosticEngine {
public:
    static NetworkMetrics performFullTest(const char* targetHost);
    static EnhancedMetrics performDeepAnalysis(const char* targetHost);
    
private:
    static int measureDNS(const char* host);
    static CongestionRating calculateCongestion(int neighbors, int overlapping);
    static int measureThroughput(const char* host);
};

#endif