#include "ConfigManager.h"
#include <ArduinoJson.h>

static Preferences configPrefs;

void ConfigManager::begin() {
    configPrefs.begin("sys-cfg", false);
}

SystemConfig ConfigManager::load() {
    SystemConfig cfg;
    configPrefs.begin("sys-cfg", true);
    
    String server = configPrefs.getString("mq_srv", "");
    cfg.mqttPort = configPrefs.getInt("mq_port", 1883);
    String tel = configPrefs.getString("mq_tel", "campus/probes/telemetry");
    String cmd = configPrefs.getString("mq_cmd", "campus/probes/cmd");
    cfg.reportInterval = configPrefs.getInt("interval", 30);
    
    configPrefs.end();

    strncpy(cfg.mqttServer, server.c_str(), 64);
    strncpy(cfg.telemetryTopic, tel.c_str(), 128);
    strncpy(cfg.cmdTopic, cmd.c_str(), 128);
    
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
    
    configPrefs.putString("mq_srv", cfg.mqttServer);
    configPrefs.putInt("mq_port", cfg.mqttPort);
    configPrefs.putString("mq_tel", cfg.telemetryTopic);
    configPrefs.putString("mq_cmd", cfg.cmdTopic);
    configPrefs.putInt("interval", cfg.reportInterval);

    configPrefs.end();
}

void ConfigManager::updateFromJSON(String json) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) return;

    SystemConfig current = load();

    if (doc.containsKey("srv")) strncpy(current.mqttServer, doc["srv"], 64);
    if (doc.containsKey("port")) current.mqttPort = doc["port"];
    if (doc.containsKey("tel")) strncpy(current.telemetryTopic, doc["tel"], 128);
    if (doc.containsKey("int")) current.reportInterval = doc["int"];

    save(current);
    Serial.println("[CONFIG] Settings updated. Rebooting...");
    delay(2000);
    ESP.restart();
}