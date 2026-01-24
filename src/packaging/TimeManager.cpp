#include "TimeManager.h"

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";
const char* ntpServer3 = "time.cloudflare.com";
const long  gmtOffset_sec = 3 * 3600;
const int   daylightOffset_sec = 0;

void TimeManager::begin() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
}

void TimeManager::sync() {
    struct tm timeinfo;
    int retries = 0;
    const int maxRetries = 10;
    
    while (!getLocalTime(&timeinfo) && retries < maxRetries) {
        Serial.print(".");
        delay(500);
        retries++;
    }
    
    if (retries >= maxRetries) {
        Serial.println("\n[TIME] Failed to obtain time");
        return;
    }
    
    Serial.println("\n[TIME] Clock Synchronized.");
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
    if(!getLocalTime(&timeinfo)) return "1970-01-01 00:00:00";
    
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}