#include "ConfigManager.h"

Preferences ConfigManager::prefs;

void ConfigManager::begin() {
    prefs.begin("campus_config", false);
}

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

bool ConfigManager::updateFromJSON(String json) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);
    
    if(error) return false;
    
    if(doc.containsKey("probe_id")) {
        setProbeId(doc["probe_id"].as<String>());
    }
    
    if(doc.containsKey("wifi")) {
        String ssid = doc["wifi"]["ssid"] | "";
        String pass = doc["wifi"]["password"] | "";
        if(ssid.length() > 0) setWifi(ssid, pass);
    }
    
    if(doc.containsKey("mqtt")) {
        String broker = doc["mqtt"]["broker"] | "";
        int port = doc["mqtt"]["port"] | 1883;
        String user = doc["mqtt"]["user"] | "";
        String pass = doc["mqtt"]["password"] | "";
        setMqtt(broker, port, user, pass);
    }
    
    if(doc.containsKey("telemetry_topic")) {
        prefs.putString("telemetry_topic", doc["telemetry_topic"].as<String>());
    }
    
    if(doc.containsKey("cmd_topic")) {
        prefs.putString("cmd_topic", doc["cmd_topic"].as<String>());
    }
    
    if(doc.containsKey("report_interval")) {
        prefs.putInt("report_interval", doc["report_interval"].as<int>());
    }
    
    return true;
}

bool ConfigManager::isConfigured() {
    return (getWifiSSID().length() > 0 && 
            getWifiPassword().length() > 0 && 
            getMqttBroker().length() > 0);
}