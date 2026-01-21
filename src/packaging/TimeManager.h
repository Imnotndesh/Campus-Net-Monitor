#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>

class TimeManager {
public:
    static void begin();
    static void sync();
    static String getTimestamp(); // Returns ISO 8601 string or Unix epoch
    static uint32_t getEpoch();
};

#endif