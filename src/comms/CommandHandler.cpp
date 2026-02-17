#include "CommandHandler.h"
#include "MqttManager.h"
#include "../diagnostics/DiagnosticEngine.h"
#include "../packaging/JsonPackager.h"
#include "../storage/ConfigManager.h"

extern SystemConfig activeCfg; 

void CommandHandler::begin() {
    Serial.println("[CMD] Command Handler Initialized");
}

void CommandHandler::process(PendingCommand cmd) {
    Serial.printf("[CMD] ║ PROCESSING: %s (ID: %s)\n", cmd.type.c_str(), cmd.id.c_str());

    if (cmd.type == "deep_scan") {
        handleDeepScan(cmd);
    } 
    else if (cmd.type == "config_update") {
        handleConfigUpdate(cmd);
    }
    else if (cmd.type == "get_config") {
        handleGetConfig(cmd);
    }
    else if (cmd.type == "set_wifi") {
        handleSetWifi(cmd);
    }
    else if (cmd.type == "set_mqtt") {
        handleSetMqtt(cmd);
    }
    else if (cmd.type == "rename_probe") {
        handleRenameProbe(cmd);
    }
    else if (cmd.type == "restart" || cmd.type == "reboot") {
        handleRestart(cmd);
    }
    else if (cmd.type == "ota_update") {
        handleOTAUpdate(cmd);
    }
    else if (cmd.type == "factory_reset") {
        handleFactoryReset(cmd);
    }
    else if (cmd.type == "ping") {
        handlePing(cmd);
    }
    else if (cmd.type == "get_status") {
        handleGetStatus(cmd);
    }
    else {
        Serial.println("[CMD]  Unknown command: " + cmd.type);
        MqttManager::publishCommandResult(cmd.type, "failed", "{\"error\": \"Unknown command type\"}", cmd.id);
    }
}

void CommandHandler::handleDeepScan(PendingCommand cmd) {
    MqttManager::publishCommandResult("deep_scan", "processing", cmd.id, "{\"msg\": \"Scan initiated\"}");
    
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, cmd.payload);
    int duration = doc["duration"] | 5;

    Serial.println("[CMD] Starting deep analysis...");
    EnhancedMetrics em = DiagnosticEngine::performDeepAnalysis("www.google.com");
    String probeId = String(ConfigManager::load().probe_id);
    
    String resultPayload = JsonPackager::serializeEnhanced(em, probeId); 
    
    MqttManager::publishCommandResult("deep_scan", "completed", resultPayload, cmd.id);
    Serial.println("[CMD] DEEP SCAN HANDLER COMPLETED");
}

void CommandHandler::handleConfigUpdate(PendingCommand cmd) {
    if (ConfigManager::updateFromJSON(cmd.payload)) {
        MqttManager::publishCommandResult("config_update", "completed", "{\"msg\": \"Config updated. Rebooting.\"}", cmd.id);
        delay(1000);
        ESP.restart();
    } else {
        MqttManager::publishCommandResult("config_update", "failed", "{\"error\": \"Invalid config JSON\"}", cmd.id);
    }
}

void CommandHandler::handleGetConfig(PendingCommand cmd) {
    String configJson = ConfigManager::getSafeConfigJson(); 
    MqttManager::publishCommandResult("get_config", "completed", configJson, cmd.id);
}

void CommandHandler::handleSetWifi(PendingCommand cmd) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, cmd.payload);
    
    const char* ssid = doc["ssid"];
    const char* pass = doc["password"];

    if (!ssid || !pass) {
        MqttManager::publishCommandResult("set_wifi", "failed", "{\"error\": \"Missing SSID/Password\"}", cmd.id);
        return;
    }
    WifiCredentials oldCreds = StorageManager::loadWifiCredentials();
    StorageManager::saveWifiCredentials(String(ssid), String(pass));
    
    MqttManager::publishCommandResult("set_wifi", "processing", "{\"msg\": \"Testing new WiFi credentials...\"}", cmd.id);
    delay(500);
    WiFi.disconnect();
    delay(1000);
    
    WiFi.begin(ssid, pass);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        MqttManager::publishCommandResult("set_wifi", "completed", "{\"msg\": \"WiFi updated successfully. IP: " + WiFi.localIP().toString() + "\"}", cmd.id);
        delay(1000);
        ESP.restart();
    } else {
        StorageManager::saveWifiCredentials(oldCreds.ssid, oldCreds.password);
        
        WiFi.disconnect();
        delay(500);
        WiFi.begin(oldCreds.ssid.c_str(), oldCreds.password.c_str());
        
        attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            attempts++;
        }
        
        MqttManager::publishCommandResult("set_wifi", "failed", "{\"error\": \"New WiFi credentials failed. Rolled back to previous working config.\"}", cmd.id);
    }
}

void CommandHandler::handleSetMqtt(PendingCommand cmd) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, cmd.payload);

    if (!doc["broker"] || !doc["port"]) {
        MqttManager::publishCommandResult("set_mqtt", "failed", "{\"error\": \"Missing Broker/Port\"}", cmd.id);
        return;
    }
    SystemConfig oldConfig = ConfigManager::load();
    ConfigManager::setMqtt(
        doc["broker"].as<String>(), 
        doc["port"].as<int>(), 
        doc["user"] | "", 
        doc["password"] | ""
    );
    
    MqttManager::publishCommandResult("set_mqtt", "processing", "{\"msg\": \"Testing new MQTT config...\"}", cmd.id);
    delay(500);
    WiFiClient testClient;
    PubSubClient testMqtt(testClient);
    testMqtt.setServer(doc["broker"].as<String>().c_str(), doc["port"].as<int>());
    
    String clientId = "ESP32-Test-" + String(random(0xffff), HEX);
    bool connected = testMqtt.connect(clientId.c_str());
    
    if (connected) {
        testMqtt.disconnect();
        MqttManager::publishCommandResult("set_mqtt", "completed", "{\"msg\": \"MQTT config saved. Rebooting...\"}", cmd.id);
        delay(1000);
        ESP.restart();
    } else {
        ConfigManager::setMqtt(
            String(oldConfig.mqttServer),
            oldConfig.mqttPort,
            "",
            ""
        );
        MqttManager::publishCommandResult("set_mqtt", "failed", "{\"error\": \"Could not connect to new MQTT broker. Rolled back to previous config.\"}", cmd.id);
    }
}

void CommandHandler::handleRenameProbe(PendingCommand cmd) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, cmd.payload);
    
    if (doc["new_id"]) {
        // Assumes setProbeId() is implemented in ConfigManager
        ConfigManager::setProbeId(doc["new_id"].as<String>());
        MqttManager::publishCommandResult("rename_probe", "completed", "{\"msg\": \"Probe renamed. Rebooting...\"}", cmd.id);
        delay(1000);
        ESP.restart();
    } else {
        MqttManager::publishCommandResult("rename_probe", "failed", "{\"error\": \"Missing new_id\"}", cmd.id);
    }
}


void CommandHandler::handleRestart(PendingCommand cmd) {
    StaticJsonDocument<128> doc;
    deserializeJson(doc, cmd.payload);
    int delayMs = doc["delay"] | 2000;

    MqttManager::publishCommandResult("restart", "completed", "{\"msg\": \"Rebooting...\"}", cmd.id);
    Serial.printf("[CMD] Rebooting in %d ms\n", delayMs);
    
    delay(delayMs);
    ESP.restart();
}

void CommandHandler::handleOTAUpdate(PendingCommand cmd) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, cmd.payload);
    
    const char* url = doc["url"];

    if (!url || strlen(url) == 0) {
        MqttManager::publishCommandResult("ota_update", "failed", "{\"error\": \"Missing URL\"}", cmd.id);
        return;
    }

    OTAManager::setStartCallback([](const char* cmdId) {
        String payload = "{\"msg\": \"OTA started\"}";
        MqttManager::publishCommandResult("ota_update", "processing", payload, String(cmdId));
    });

    OTAManager::setProgressCallback([](int current, int total, const char* cmdId) {
        int percent = (current * 100) / total;
        String payload = "{\"progress\": " + String(percent) + ", \"current\": " + String(current) + ", \"total\": " + String(total) + "}";
        MqttManager::publishCommandResult("ota_update", "processing", payload, String(cmdId));
    });

    OTAManager::setErrorCallback([](int error, const char* cmdId) {
        String payload = "{\"error\": \"OTA failed\", \"code\": " + String(error) + "}";
        MqttManager::publishCommandResult("ota_update", "failed", payload, String(cmdId));
    });

    OTAManager::setFinishCallback([](const char* cmdId) {
        String payload = "{\"msg\": \"OTA completed, rebooting...\"}";
        MqttManager::publishCommandResult("ota_update", String(cmdId), "completed", payload);
    });
    bool success = OTAManager::performUpdate(url, cmd.id.c_str());
    
    if (success) {
        delay(2000);
        ESP.restart();
    }
}
void CommandHandler::handleFactoryReset(PendingCommand cmd) {
    MqttManager::publishCommandResult("factory_reset", "processing", "{\"msg\": \"Wiping data...\"}", cmd.id);
    StorageManager::wipe();
    delay(1000);
    ESP.restart();
}

void CommandHandler::handlePing(PendingCommand cmd) {
    StaticJsonDocument<512> doc;
    doc["type"] = "pong";
    doc["uptime"] = millis() / 1000;
    doc["rssi"] = WiFi.RSSI();
    
    String res;
    serializeJson(doc, res);
    MqttManager::publishCommandResult("ping", "completed", res, cmd.id);
}

void CommandHandler::handleGetStatus(PendingCommand cmd) {
    StaticJsonDocument<512> status;
    status["uptime"] = millis() / 1000;
    status["free_heap"] = ESP.getFreeHeap();
    status["rssi"] = WiFi.RSSI();
    status["ip"] = WiFi.localIP().toString();
    status["ssid"] = WiFi.SSID(); // Added SSID
    
    String res;
    serializeJson(status, res);
    MqttManager::publishCommandResult("get_status", "completed", res, cmd.id);
}