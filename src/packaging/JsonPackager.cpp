#include "JsonPackager.h"
#include "../packaging/TimeManager.h"

String JsonPackager::serializeLight(const NetworkMetrics& m, String probeId) {
    StaticJsonDocument<384> doc;
    
    doc["pid"] = probeId;
    doc["type"] = "light";
    doc["ts"] = TimeManager::getTimestamp();
    doc["epoch"] = TimeManager::getEpoch();
    doc["rssi"] = m.rssi;
    doc["lat"] = m.avgLatency;
    doc["loss"] = m.packetLoss;
    doc["dns"] = m.dnsResolutionTime;
    doc["ch"] = m.channel;
    doc["cong"] = (int)m.congestion;
    doc["bssid"] = m.bssid;
    doc["neighbors"] = m.neighborCount;
    doc["overlap"] = m.overlappingCount;
    
    String output;
    serializeJson(doc, output);
    return output;
}

String JsonPackager::serializeEnhanced(const EnhancedMetrics& em, String probeId) {
    StaticJsonDocument<640> doc;
    
    doc["pid"] = probeId;
    doc["type"] = "enhanced";
    doc["ts"] = TimeManager::getTimestamp();
    doc["epoch"] = TimeManager::getEpoch();
    doc["rssi"] = em.rssi;
    doc["snr"] = em.snr;
    doc["qual"] = em.linkQuality;
    doc["util"] = em.channelUtilization;
    doc["phy"] = em.phyMode;
    doc["tput"] = em.tcpThroughput;
    doc["up"] = em.uptime;
    doc["bssid"] = em.bssid;
    doc["ch"] = em.channel;
    doc["noise"] = em.noiseFloor;
    doc["lat"] = em.avgLatency;
    doc["loss"] = em.packetLoss;
    doc["dns"] = em.dnsResolutionTime;

    String output;
    serializeJson(doc, output);
    return output;
}