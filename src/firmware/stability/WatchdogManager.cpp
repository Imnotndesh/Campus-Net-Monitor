#include "WatchdogManager.h"

void WatchdogManager::begin(uint32_t timeoutSeconds) {
    Serial.printf("[SYSTEM] Initializing Watchdog (%d seconds)...\n", timeoutSeconds);
    esp_task_wdt_init(timeoutSeconds, true);
    esp_task_wdt_add(NULL);
}

void WatchdogManager::reset() {
    esp_task_wdt_reset();
}