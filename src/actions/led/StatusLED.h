#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>

enum SystemStatus {
    STATUS_OK = 0,         // Solid ON
    ERR_WIFI = 4,         // 4 Pulses: WiFi Connection Failed
    ERR_MQTT = 5,         // 5 Pulses: MQTT Server Unreachable
    ERR_GENERAL = 2,      // 2 Pulses: General System Error
    MODE_PORTAL = 1       // 1 Pulse (Slow): Setup Mode Active
};

class StatusLED {
public:
    static void begin(int pin);
    static void setStatus(SystemStatus status);
    static void update(); 
private:
    static int _pin;
    static SystemStatus _currentStatus;
    static unsigned long _lastAction;
    static int _pulseCount;
    static bool _isPaused;
};
#endif