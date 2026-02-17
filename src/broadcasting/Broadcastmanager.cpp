#include "BroadcastManager.h"

SystemConfig* BroadcastManager::activeConfig = nullptr;

void BroadcastManager::begin(SystemConfig* config) {
    activeConfig = config;
    xTaskCreatePinnedToCore(
        broadcastTask,
        "broadcastTask",
        4096,
        NULL,
        1,
        NULL,
        0 
    );
    
    Serial.println("[BROADCAST] Manager initialized on Core 0");
}

void BroadcastManager::broadcastTask(void* pvParameters) {
    unsigned long lastStatusBroadcast = millis();
    unsigned long lastConfigBroadcast = millis();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    
    for(;;) {
        if (MqttManager::isConnected()) {
            if (millis() - lastStatusBroadcast > 120000) {
                lastStatusBroadcast = millis();
                broadcastStatus();
            }
            
            if (millis() - lastConfigBroadcast > 300000) {
                lastConfigBroadcast = millis();
                broadcastConfig();
            }
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void BroadcastManager::broadcastStatus() {
    if (!activeConfig) return;
    
    Serial.println("[STATUS] Broadcasting status update");
    
    StaticJsonDocument<512> doc;
    doc["probe_id"] = activeConfig->probe_id;
    doc["type"] = "status_broadcast";
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
    doc["ssid"] = WiFi.SSID();
    doc["temp_c"] = temperatureRead();
    doc["timestamp"] = TimeManager::getTimestamp();
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = "campus/probes/" + String(activeConfig->probe_id) + "/status";
    
    if (MqttManager::publishBroadcast(topic, payload)) {
        Serial.println("[STATUS] ✓ Broadcast published");
    } else {
        Serial.println("[STATUS] ✗ Broadcast failed");
    }
}

void BroadcastManager::broadcastConfig() {
    if (!activeConfig) return;
    
    Serial.println("[CONFIG] Broadcasting config update");
    
    String configJson = ConfigManager::getSafeConfigJson();
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, configJson);
    doc["type"] = "config_broadcast";
    doc["timestamp"] = TimeManager::getTimestamp();
    
    String payload;
    serializeJson(doc, payload);
    
    String topic = "campus/probes/" + String(activeConfig->probe_id) + "/config";
    
    if (MqttManager::publishBroadcast(topic, payload)) {
        Serial.println("[CONFIG] ✓ Broadcast published");
    } else {
        Serial.println("[CONFIG] ✗ Broadcast failed");
    }
}