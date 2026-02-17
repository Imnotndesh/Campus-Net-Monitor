// ConfigManager.h
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>

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
    static String getProbeId();
    static String getWifiSSID();
    static String getWifiPassword();
    static String getMqttBroker();
    static int getMqttPort();
    static String getMqttUser();
    static String getMqttPassword();
    static String getSafeConfigJson();
    static void setWifi(String ssid, String password);
    static void setMqtt(String broker, int port, String user, String password);
    static void setProbeId(String newId);
    
private:
    static Preferences prefs;
};
#endif