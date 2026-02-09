#include "CommandHandler.h"

void CommandHandler::begin() {
    Serial.println("[CMD] Command Handler Ready");
}

void CommandHandler::process(PendingCommand cmd) {
    Serial.println("[CMD] Processing: " + cmd.type);

    if (cmd.type == "deep_scan") {
        handleDeepScan(cmd.payload);
    } 
    else if (cmd.type == "config_update") {
        handleConfigUpdate(cmd.payload);
    }
    else if (cmd.type == "restart" || cmd.type == "reboot") {
        handleRestart(cmd.payload);
    }
    else if (cmd.type == "ota_update") {
        handleOTAUpdate(cmd.payload);
    }
    else if (cmd.type == "factory_reset") {
        handleFactoryReset(cmd.payload);
    }
    else if (cmd.type == "ping") {
        handlePing(cmd.payload);
    }
    else if (cmd.type == "get_status") {
        handleGetStatus(cmd.payload);
    }
    else {
        Serial.println("[CMD] Unknown command: " + cmd.type);
        MqttManager::publishCommandResult(cmd.type, "failed", "{\"error\": \"Unknown command\"}");
    }
}
void CommandHandler::handleDeepScan(String payload) {
    MqttManager::publishCommandResult("deep_scan", "processing", "{\"msg\": \"Scan started\"}");
    EnhancedMetrics em = DiagnosticEngine::performDeepAnalysis("8.8.8.8");
    String probeId = String(ConfigManager::load().probe_id);
    String resultPayload = JsonPackager::serializeEnhanced(em, probeId);
    Serial.printf("[CMD] Deep Scan Payload Size: %d bytes\n", resultPayload.length());
    MqttManager::publishCommandResult("deep_scan", "completed", resultPayload);
}
void CommandHandler::handleConfigUpdate(String payload) {
    if (ConfigManager::updateFromJSON(payload)) {
        MqttManager::publishCommandResult("config_update", "completed", "{\"msg\": \"Configuration updated. Rebooting.\"}");
        delay(1000);
        ESP.restart();
    } else {
        MqttManager::publishCommandResult("config_update", "failed", "{\"error\": \"Failed to parse or apply config\"}");
    }
}
void CommandHandler::handleRestart(String payload) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);
    int delayMs = doc["delay"] | 2000;

    MqttManager::publishCommandResult("restart", "completed", "{\"msg\": \"Rebooting...\"}");
    Serial.printf("[CMD] Rebooting in %d ms\n", delayMs);
    
    delay(delayMs);
    ESP.restart();
}
void CommandHandler::handleOTAUpdate(String payload) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    const char* url = doc["url"];
    const char* version = doc["version"];

    if (url && strlen(url) > 0) {
        MqttManager::publishCommandResult("ota_update", "processing", "{\"msg\": \"Starting OTA...\"}");
        OTAManager::checkForUpdates(url, version);
    } else {
        MqttManager::publishCommandResult("ota_update", "failed", "{\"error\": \"Missing URL\"}");
    }
}
void CommandHandler::handleFactoryReset(String payload) {
    MqttManager::publishCommandResult("factory_reset", "processing", "{\"msg\": \"Wiping data...\"}");
    StorageManager::wipe();
    delay(1000);
    ESP.restart();
}
void CommandHandler::handlePing(String payload) {
    StaticJsonDocument<512> doc;
    doc["type"] = "pong";
    doc["uptime"] = millis() / 1000;
    doc["rssi"] = WiFi.RSSI();
    
    String res;
    serializeJson(doc, res);
    MqttManager::publishCommandResult("ping", "completed", res);
}
void CommandHandler::handleGetStatus(String payload) {
    StaticJsonDocument<512> status;
    status["uptime"] = millis() / 1000;
    status["free_heap"] = ESP.getFreeHeap();
    status["rssi"] = WiFi.RSSI();
    status["ip"] = WiFi.localIP().toString();
    String res;
    serializeJson(status, res);
    MqttManager::publishCommandResult("get_status", "completed", res);
}