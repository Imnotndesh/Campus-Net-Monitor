#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <Arduino.h>
#include <esp_task_wdt.h>

class WatchdogManager {
public:
    static void begin(uint32_t timeoutSeconds);
    static void reset();
};

#endif