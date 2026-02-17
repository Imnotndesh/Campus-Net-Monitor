#ifndef BROADCAST_MANAGER_H
#define BROADCAST_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../storage/ConfigManager.h"
#include "../comms/MqttManager.h"
#include "../packaging/TimeManager.h"

class BroadcastManager {
public:
    static void begin(SystemConfig* config);
    static void broadcastTask(void* pvParameters);
    
private:
    static void broadcastStatus();
    static void broadcastConfig();
    static SystemConfig* activeConfig;
};

#endif