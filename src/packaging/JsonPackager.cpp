#include "JsonPackager.h"

String JsonPackager::serializeLight(const NetworkMetrics& m, String probeId) {
    StaticJsonDocument<256> doc;
    
    doc["pid"] = probeId;
    doc["type"] = "light";
    doc["rssi"] = m.rssi;
    doc["lat"] = m.avgLatency;
    doc["loss"] = m.packetLoss;
    doc["dns"] = m.dnsResolutionTime;
    doc["ch"] = m.channel;
    doc["cong"] = (int)m.congestion;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String JsonPackager::serializeEnhanced(const EnhancedMetrics& em, String probeId) {
    StaticJsonDocument<512> doc; 
    
    doc["pid"] = probeId;
    doc["type"] = "enhanced";
    doc["rssi"] = em.rssi;
    doc["snr"] = em.snr;
    doc["qual"] = em.linkQuality;
    doc["util"] = em.channelUtilization;
    doc["phy"] = em.phyMode;
    doc["tput"] = em.tcpThroughput;
    doc["up"] = em.uptime;

    String output;
    serializeJson(doc, output);
    return output;
}