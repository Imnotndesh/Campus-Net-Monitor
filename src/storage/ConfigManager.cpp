#include "ConfigManager.h"
#include <ArduinoJson.h>

static Preferences configPrefs;

void ConfigManager::begin() {
    configPrefs.begin("sys-cfg", false);
}

SystemConfig ConfigManager::load() {
    SystemConfig cfg;
    configPrefs.begin("sys-cfg", true); // Read-only mode
    
    // Load Probe ID (Default to empty/unknown)
    String pid = configPrefs.getString("probe_id", "PROBE-NEW");
    strncpy(cfg.probe_id, pid.c_str(), 32);

    // Load MQTT Settings
    String server = configPrefs.getString("mq_srv", "");
    strncpy(cfg.mqttServer, server.c_str(), 64);
    
    cfg.mqttPort = configPrefs.getInt("mq_port", 1883);
    
    String tel = configPrefs.getString("mq_tel", "campus/probes/telemetry");
    strncpy(cfg.telemetryTopic, tel.c_str(), 128);
    
    String cmd = configPrefs.getString("mq_cmd", "campus/probes/cmd");
    strncpy(cfg.cmdTopic, cmd.c_str(), 128);
    
    cfg.reportInterval = configPrefs.getInt("interval", 30);
    
    configPrefs.end();
    return cfg;
}

bool ConfigManager::isConfigured() {
    configPrefs.begin("sys-cfg", true);
    bool hasKey = configPrefs.isKey("mq_srv");
    String srv = configPrefs.getString("mq_srv", "");
    configPrefs.end();
    return (hasKey && srv.length() > 0);
}

void ConfigManager::save(SystemConfig cfg) {
    configPrefs.begin("sys-cfg", false);
    
    configPrefs.putString("probe_id", cfg.probe_id);
    configPrefs.putString("mq_srv", cfg.mqttServer);
    configPrefs.putInt("mq_port", cfg.mqttPort);
    configPrefs.putString("mq_tel", cfg.telemetryTopic);
    configPrefs.putString("mq_cmd", cfg.cmdTopic);
    configPrefs.putInt("interval", cfg.reportInterval);

    configPrefs.end();
}

bool ConfigManager::updateFromJSON(String json) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        Serial.println("[CONFIG] JSON Parse Error");
        return false;
    }

    SystemConfig current = load();
    bool changed = false;

    // Match keys with Go Backend (CommandService.go)
    if (doc.containsKey("probe_id")) {
        strncpy(current.probe_id, doc["probe_id"], 32);
        changed = true;
    }
    if (doc.containsKey("mqtt_server")) {
        strncpy(current.mqttServer, doc["mqtt_server"], 64);
        changed = true;
    }
    if (doc.containsKey("mqtt_port")) {
        current.mqttPort = doc["mqtt_port"];
        changed = true;
    }
    if (doc.containsKey("telemetry_topic")) {
        strncpy(current.telemetryTopic, doc["telemetry_topic"], 128);
        changed = true;
    }
    if (doc.containsKey("report_interval")) {
        current.reportInterval = doc["report_interval"];
        changed = true;
    }

    // Also support the short keys just in case ConnectionManager uses them
    if (doc.containsKey("srv")) { strncpy(current.mqttServer, doc["srv"], 64); changed = true; }
    if (doc.containsKey("int")) { current.reportInterval = doc["int"]; changed = true; }
    
    if (changed) {
        save(current);
        Serial.println("[CONFIG] Settings updated.");
        return true;
    }
    
    return false;
}