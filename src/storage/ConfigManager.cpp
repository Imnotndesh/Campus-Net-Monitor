#include "ConfigManager.h"

Preferences ConfigManager::prefs;

void ConfigManager::begin() {
    prefs.begin("campus_config", false);
        Preferences fleetPrefs;
    fleetPrefs.begin("fleet", false);
    fleetPrefs.end();
}
bool ConfigManager::_fleetManagedCache = false;
bool ConfigManager::_fleetCacheValid = false;
String ConfigManager::getProbeId() { return prefs.getString("probe_id", "PROBE-DEFAULT"); }
String ConfigManager::getWifiSSID() { return prefs.getString("wifi_ssid", ""); }
String ConfigManager::getWifiPassword() { return prefs.getString("wifi_pass", ""); }
String ConfigManager::getMqttBroker() { return prefs.getString("mqtt_broker", ""); }
int ConfigManager::getMqttPort() { return prefs.getInt("mqtt_port", 1883); }
String ConfigManager::getMqttUser() { return prefs.getString("mqtt_user", ""); }
String ConfigManager::getMqttPassword() { return prefs.getString("mqtt_pass", ""); }

void ConfigManager::setWifi(String ssid, String password) {
    if(ssid.length() > 0) prefs.putString("wifi_ssid", ssid);
    if(password.length() > 0) prefs.putString("wifi_pass", password);
}

void ConfigManager::setMqtt(String broker, int port, String user, String password) {
    if(broker.length() > 0) prefs.putString("mqtt_broker", broker);
    if(port > 0) prefs.putInt("mqtt_port", port);
    if(user.length() > 0) prefs.putString("mqtt_user", user);
    if(password.length() > 0) prefs.putString("mqtt_pass", password);
}

void ConfigManager::setProbeId(String newId) {
    if(newId.length() > 0) prefs.putString("probe_id", newId);
}

String ConfigManager::getSafeConfigJson() {
    DynamicJsonDocument doc(1024);
    
    doc["probe_id"] = getProbeId();
    Preferences wifiPrefs;
    wifiPrefs.begin("wifi-creds", true);
    String wifiSsid = wifiPrefs.getString("ssid", "");
    String wifiPass = wifiPrefs.getString("pass", "");
    wifiPrefs.end();
    
    doc["wifi"]["ssid"] = wifiSsid;
    doc["wifi"]["configured"] = (wifiPass.length() > 0);

    doc["mqtt"]["broker"] = getMqttBroker();
    doc["mqtt"]["port"] = getMqttPort();
    doc["mqtt"]["user"] = getMqttUser();
    doc["heap_free"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    
    doc["temp_c"] = temperatureRead(); 

    String output;
    serializeJson(doc, output);
    return output;
}

SystemConfig ConfigManager::load() {
    SystemConfig config;
    String probeId = getProbeId();
    strncpy(config.probe_id, probeId.c_str(), sizeof(config.probe_id) - 1);
    config.probe_id[sizeof(config.probe_id) - 1] = '\0';
    
    String mqttBroker = getMqttBroker();
    strncpy(config.mqttServer, mqttBroker.c_str(), sizeof(config.mqttServer) - 1);
    config.mqttServer[sizeof(config.mqttServer) - 1] = '\0';
    
    config.mqttPort = getMqttPort();
    
    String telemetryTopic = prefs.getString("telemetry_topic", "campus/telemetry/" + probeId);
    strncpy(config.telemetryTopic, telemetryTopic.c_str(), sizeof(config.telemetryTopic) - 1);
    config.telemetryTopic[sizeof(config.telemetryTopic) - 1] = '\0';
    
    String cmdTopic = prefs.getString("cmd_topic", "campus/cmd/" + probeId);
    strncpy(config.cmdTopic, cmdTopic.c_str(), sizeof(config.cmdTopic) - 1);
    config.cmdTopic[sizeof(config.cmdTopic) - 1] = '\0';
    
    config.reportInterval = prefs.getInt("report_interval", 60);    
    return config;
}

void ConfigManager::save(SystemConfig config) {
    prefs.putString("probe_id", config.probe_id);
    prefs.putString("mqtt_broker", config.mqttServer);
    prefs.putInt("mqtt_port", config.mqttPort);
    prefs.putString("telemetry_topic", config.telemetryTopic);
    prefs.putString("cmd_topic", config.cmdTopic);
    prefs.putInt("report_interval", config.reportInterval);
}

// In ConfigManager.cpp
bool ConfigManager::updateFromJSON(const String& json) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);
    if (error) return false;
    if (doc.containsKey("probe_id")) {
        setProbeId(doc["probe_id"].as<String>());
    }
    if (doc.containsKey("wifi")) {
        JsonObject wifi = doc["wifi"];
        if (!wifi["ssid"].is<String>() || !wifi["password"].is<String>()) {
            return false;
        }
        String ssid = wifi["ssid"].as<String>();
        String pass = wifi["password"].as<String>();
        if (ssid.length() == 0 || pass.length() == 0) {
            return false; // reject empty strings
        }
        setWifi(ssid, pass);
    }
    if (doc.containsKey("mqtt")) {
        JsonObject mqtt = doc["mqtt"];
        if (!mqtt["broker"].is<String>() || mqtt["broker"].as<String>().length() == 0) {
            return false;
        }
        String broker = mqtt["broker"].as<String>();
        int port = mqtt["port"] | 1883;
        String user = mqtt["user"] | "";
        String pass = mqtt["password"] | "";
        setMqtt(broker, port, user, pass);
    }

    // --- Telemetry and command topics ---
    if (doc.containsKey("telemetry_topic")) {
        prefs.putString("telemetry_topic", doc["telemetry_topic"].as<String>());
    }
    if (doc.containsKey("cmd_topic")) {
        prefs.putString("cmd_topic", doc["cmd_topic"].as<String>());
    }

    // --- Report interval ---
    if (doc.containsKey("report_interval")) {
        prefs.putInt("report_interval", doc["report_interval"].as<int>());
    }

    // --- Fleet Location ---
    if (doc.containsKey("location")) {
        setFleetLocation(doc["location"].as<String>());
    }

    // --- Fleet Groups (comma‑separated string) ---
    if (doc.containsKey("groups")) {
        String groups;
        if (doc["groups"].is<JsonArray>()) {
            JsonArray arr = doc["groups"].as<JsonArray>();
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) groups += ",";
                groups += arr[i].as<String>();
            }
        } else {
            groups = doc["groups"].as<String>();
        }
        setFleetGroups(groups);
    }

    // --- Fleet Tags (JSON object) ---
    if (doc.containsKey("tags")) {
        String tags;
        serializeJson(doc["tags"], tags);
        setFleetTags(tags);
    }

    return true;
}

void ConfigManager::setFleetGroups(String groups) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {  // false = read/write mode
        fleetPrefs.putString("groups", groups);
        fleetPrefs.end();
    } else {
        Serial.println("[CONFIG] Failed to open fleet namespace for writing");
    }
}

String ConfigManager::getFleetGroups() {
    Preferences fleetPrefs;
    String groups = "";
    if (fleetPrefs.begin("fleet", true)) {  // true = read-only mode
        groups = fleetPrefs.getString("groups", "");
        fleetPrefs.end();
    }
    return groups;
}

void ConfigManager::setFleetLocation(String location) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.putString("location", location);
        fleetPrefs.end();
    }
}

String ConfigManager::getFleetLocation() {
    Preferences fleetPrefs;
    String location = "";
    if (fleetPrefs.begin("fleet", true)) {
        location = fleetPrefs.getString("location", "");
        fleetPrefs.end();
    }
    return location;
}

void ConfigManager::setFleetTags(String tags) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.putString("tags", tags);
        fleetPrefs.end();
    }
}

String ConfigManager::getFleetTags() {
    Preferences fleetPrefs;
    String tags = "{}";
    if (fleetPrefs.begin("fleet", true)) {
        tags = fleetPrefs.getString("tags", "{}");
        fleetPrefs.end();
    }
    return tags;
}

void ConfigManager::setFleetManaged(bool managed) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.putBool("managed", managed);
        fleetPrefs.end();
    }
    _fleetManagedCache = managed;
    _fleetCacheValid = true;
}

bool ConfigManager::isFleetManaged() {
    if (!_fleetCacheValid) {
        Preferences fleetPrefs;
        if (fleetPrefs.begin("fleet", true)) {
            _fleetManagedCache = fleetPrefs.getBool("managed", false);
            fleetPrefs.end();
        }
        _fleetCacheValid = true;
    }
    return _fleetManagedCache;
}

void ConfigManager::setMaintenanceWindow(String window) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.putString("maint_window", window);
        fleetPrefs.end();
    }
}

String ConfigManager::getMaintenanceWindow() {
    Preferences fleetPrefs;
    String window = "";
    if (fleetPrefs.begin("fleet", true)) {
        window = fleetPrefs.getString("maint_window", "");
        fleetPrefs.end();
    }
    return window;
}

void ConfigManager::setFleetConfigVersion(int version) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.putInt("config_ver", version);
        fleetPrefs.end();
    }
}

int ConfigManager::getFleetConfigVersion() {
    Preferences fleetPrefs;
    int version = 0;
    if (fleetPrefs.begin("fleet", true)) {
        version = fleetPrefs.getInt("config_ver", 0);
        fleetPrefs.end();
    }
    return version;
}

void ConfigManager::incrementFleetCommandCount() {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        int count = fleetPrefs.getInt("cmd_count", 0);
        fleetPrefs.putInt("cmd_count", count + 1);
        fleetPrefs.end();
    }
}

int ConfigManager::getFleetCommandCount() {
    Preferences fleetPrefs;
    int count = 0;
    if (fleetPrefs.begin("fleet", true)) {
        count = fleetPrefs.getInt("cmd_count", 0);
        fleetPrefs.end();
    }
    return count;
}

void ConfigManager::setLastFleetCommand(String commandId) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.putString("last_cmd", commandId);
        fleetPrefs.putULong64("last_cmd_time", millis());
        fleetPrefs.end();
    }
}

String ConfigManager::getLastFleetCommand() {
    Preferences fleetPrefs;
    String cmdId = "";
    if (fleetPrefs.begin("fleet", true)) {
        cmdId = fleetPrefs.getString("last_cmd", "");
        fleetPrefs.end();
    }
    return cmdId;
}

void ConfigManager::setFirmwareVersion(String version) {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.putString("fw_version", version);
        fleetPrefs.end();
    }
}

String ConfigManager::getFirmwareVersion() {
    Preferences fleetPrefs;
    String version = "1.0.0";
    if (fleetPrefs.begin("fleet", true)) {
        version = fleetPrefs.getString("fw_version", "1.0.0");
        fleetPrefs.end();
    }
    return version;
}

void ConfigManager::clearFleetState() {
    Preferences fleetPrefs;
    if (fleetPrefs.begin("fleet", false)) {
        fleetPrefs.clear();
        fleetPrefs.end();
    }
    _fleetManagedCache = false;
    _fleetCacheValid = true;
}