#include "TimeManager.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3 * 3600;
const int   daylightOffset_sec = 0;

void TimeManager::begin() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void TimeManager::sync() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("[TIME] Failed to obtain time");
        return;
    }
    Serial.println("[TIME] Clock Synchronized.");
}

uint32_t TimeManager::getEpoch() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return 0;
    time(&now);
    return (uint32_t)now;
}

String TimeManager::getTimestamp() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) return "0000-00-00 00:00:00";
    
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}