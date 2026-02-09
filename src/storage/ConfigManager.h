#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

struct SystemConfig {
    char probe_id[32];
    char mqttServer[64];
    int mqttPort;
    char telemetryTopic[128];
    char cmdTopic[128];
    int reportInterval;
};

class ConfigManager {
public:
    static void begin();
    static SystemConfig load();
    static void save(SystemConfig config);
    static bool updateFromJSON(String json);
    static bool isConfigured();
};

#endif