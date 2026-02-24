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
    
    static void setFleetGroups(String groups);
    static String getFleetGroups();
    static void setFleetLocation(String location);
    static String getFleetLocation();
    static void setFleetTags(String tags);
    static String getFleetTags();
    static void setFleetManaged(bool managed);
    static bool isFleetManaged();
    static void setMaintenanceWindow(String window);
    static String getMaintenanceWindow();
    static void setFleetConfigVersion(int version);
    static int getFleetConfigVersion();
    static void incrementFleetCommandCount();
    static int getFleetCommandCount();
    static void setLastFleetCommand(String commandId);
    static String getLastFleetCommand();
    static void clearFleetState();
    static void setFirmwareVersion(String version);
    static String getFirmwareVersion();
    
private:
    static Preferences prefs;
    static bool _fleetManagedCache;
    static bool _fleetCacheValid;
};
#endif