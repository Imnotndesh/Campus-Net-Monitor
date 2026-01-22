#include "WatchdogManager.h"

void WatchdogManager::begin(uint32_t timeoutSeconds) {
    Serial.printf("[SYSTEM] Initializing Watchdog (%d seconds)...\n", timeoutSeconds);
    esp_task_wdt_init(timeoutSeconds, true); // true = panic/reboot on timeout
    esp_task_wdt_add(NULL); // Add current thread (loop) to WDT
}

void WatchdogManager::reset() {
    esp_task_wdt_reset();
}