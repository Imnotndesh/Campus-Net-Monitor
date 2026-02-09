// src/comms/CommandHandler.cpp (Fixed)
#include "CommandHandler.h"
#include "MqttManager.h"
#include "../diagnostics/DiagnosticEngine.h"
#include "../packaging/JsonPackager.h"

void CommandHandler::begin() {
    Serial.println("[CMD] Command handler initialized");
}

void CommandHandler::processCommand(String topic, String payload) {
    Serial.println("[CMD] Received command: " + payload);
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.println("[CMD] JSON Parse Error: " + String(error.c_str()));
        return;
    }

    String commandType = doc["command"] | "unknown";
    String commandId = doc["command_id"] | "no-id";
    JsonObject params = doc["params"].as<JsonObject>();
    
    Serial.printf("[CMD] Processing: %s (ID: %s)\n", commandType.c_str(), commandId.c_str());

    CommandType cmd = parseCommandType(commandType);
    
    switch (cmd) {
        case CMD_DEEP_SCAN:
            handleDeepScan(params);
            sendCommandResponse(commandId, true, "Deep scan started");
            break;
            
        case CMD_CONFIG_UPDATE:
            handleConfigUpdate(params);
            sendCommandResponse(commandId, true, "Configuration updated");
            break;
            
        case CMD_RESTART:
            handleRestart(params);
            sendCommandResponse(commandId, true, "Restarting probe");
            break;
            
        case CMD_OTA_UPDATE:
            handleOTAUpdate(params);
            sendCommandResponse(commandId, true, "OTA update initiated");
            break;
            
        case CMD_FACTORY_RESET:
            handleFactoryReset(params);
            sendCommandResponse(commandId, true, "Factory reset initiated");
            break;
            
        case CMD_PING:
            handlePing(params);
            sendCommandResponse(commandId, true, "Pong");
            break;
            
        case CMD_GET_STATUS:
            handleGetStatus(params);
            sendCommandResponse(commandId, true, "Status report sent");
            break;
            
        default:
            Serial.println("[CMD] Unknown command: " + commandType);
            sendCommandResponse(commandId, false, "Unknown command");
            break;
    }
}

CommandType CommandHandler::parseCommandType(String type) {
    if (type == "deep_scan") return CMD_DEEP_SCAN;
    if (type == "config_update") return CMD_CONFIG_UPDATE;
    if (type == "restart") return CMD_RESTART;
    if (type == "ota_update") return CMD_OTA_UPDATE;
    if (type == "factory_reset") return CMD_FACTORY_RESET;
    if (type == "ping") return CMD_PING;
    if (type == "get_status") return CMD_GET_STATUS;
    return CMD_UNKNOWN;
}

void CommandHandler::handleDeepScan(JsonObject& params) {
    Serial.println("[CMD] Executing deep scan...");
    
    int duration = 2;
    if (params.containsKey("duration")) {
        duration = params["duration"];
    }
    Serial.printf("[CMD] Scan duration: %d seconds\n", duration);
    
    // Perform deep analysis
    EnhancedMetrics em = DiagnosticEngine::performDeepAnalysis("8.8.8.8");
    
    // Package and send enhanced telemetry
    String payload = JsonPackager::serializeEnhanced(em, "PROBE-SEC-05");
    
    if (MqttManager::publishTelemetry(payload)) {
        Serial.println("[CMD] Deep scan results published");
    } else {
        Serial.println("[CMD] Deep scan results buffered offline");
    }
}

void CommandHandler::handleConfigUpdate(JsonObject& params) {
    Serial.println("[CMD] Updating configuration...");
    
    SystemConfig cfg = ConfigManager::load();
    bool changed = false;
    
    if (params.containsKey("report_interval")) {
        cfg.reportInterval = params["report_interval"];
        Serial.printf("[CMD] Report interval: %d\n", cfg.reportInterval);
        changed = true;
    }
    
    if (params.containsKey("mqtt_server")) {
        const char* srv = params["mqtt_server"];
        if (strlen(srv) > 0) {
            strncpy(cfg.mqttServer, srv, 64);
            Serial.printf("[CMD] MQTT Server: %s\n", cfg.mqttServer);
            changed = true;
        }
    }
    
    if (params.containsKey("mqtt_port")) {
        cfg.mqttPort = params["mqtt_port"];
        Serial.printf("[CMD] MQTT Port: %d\n", cfg.mqttPort);
        changed = true;
    }
    
    if (params.containsKey("telemetry_topic")) {
        const char* topic = params["telemetry_topic"];
        if (strlen(topic) > 0) {
            strncpy(cfg.telemetryTopic, topic, 128);
            Serial.printf("[CMD] Telemetry topic: %s\n", cfg.telemetryTopic);
            changed = true;
        }
    }
    
    if (changed) {
        ConfigManager::save(cfg);
        Serial.println("[CMD] Configuration saved. Rebooting in 3 seconds...");
        delay(3000);
        ESP.restart();
    } else {
        Serial.println("[CMD] No configuration changes");
    }
}

void CommandHandler::handleRestart(JsonObject& params) {
    int delay_ms = 2000;
    if (params.containsKey("delay")) {
        delay_ms = params["delay"];
    }
    Serial.printf("[CMD] Restarting in %d ms\n", delay_ms);
    delay(delay_ms);
    ESP.restart();
}

void CommandHandler::handleOTAUpdate(JsonObject& params) {
    const char* url = "";
    const char* version = "unknown";
    
    if (params.containsKey("url")) {
        url = params["url"];
    }
    
    if (params.containsKey("version")) {
        version = params["version"];
    }
    
    if (strlen(url) == 0) {
        Serial.println("[CMD] OTA Error: No URL provided");
        return;
    }
    
    Serial.printf("[CMD] Starting OTA update from: %s\n", url);
    Serial.printf("[CMD] Target version: %s\n", version);
    
    OTAManager::checkForUpdates(url, version);
}

void CommandHandler::handleFactoryReset(JsonObject& params) {
    Serial.println("[CMD] FACTORY RESET INITIATED");
    Serial.println("[CMD] Wiping all stored data...");
    
    StorageManager::wipe();
    
    Serial.println("[CMD] Factory reset complete. Rebooting...");
    delay(2000);
    ESP.restart();
}

void CommandHandler::handlePing(JsonObject& params) {
    Serial.println("[CMD] Ping received, responding with pong");
    
    StaticJsonDocument<256> response;
    response["type"] = "pong";
    response["uptime"] = millis() / 1000;
    response["free_heap"] = ESP.getFreeHeap();
    response["rssi"] = WiFi.RSSI();
    
    String payload;
    serializeJson(response, payload);
    
    MqttManager::publishTelemetry(payload);
}

void CommandHandler::handleGetStatus(JsonObject& params) {
    Serial.println("[CMD] Generating status report...");
    
    StaticJsonDocument<512> status;
    
    status["probe_id"] = "PROBE-SEC-05";
    status["type"] = "status_report";
    status["timestamp"] = millis() / 1000;
    
    // System info
    status["uptime"] = millis() / 1000;
    status["free_heap"] = ESP.getFreeHeap();
    status["chip_model"] = ESP.getChipModel();
    status["chip_revision"] = ESP.getChipRevision();
    status["cpu_freq"] = ESP.getCpuFreqMHz();
    
    // Network info
    status["wifi_ssid"] = WiFi.SSID();
    status["wifi_rssi"] = WiFi.RSSI();
    status["wifi_channel"] = WiFi.channel();
    status["ip_address"] = WiFi.localIP().toString();
    status["mac_address"] = WiFi.macAddress();
    
    // Configuration
    SystemConfig cfg = ConfigManager::load();
    status["report_interval"] = cfg.reportInterval;
    status["mqtt_server"] = cfg.mqttServer;
    status["mqtt_port"] = cfg.mqttPort;
    
    // Storage
    status["failure_count"] = StorageManager::getFailureCount();
    status["buffer_size"] = StorageManager::getBufferSize();
    
    String payload;
    serializeJson(status, payload);
    
    MqttManager::publishTelemetry(payload);
}

void CommandHandler::sendCommandResponse(String commandId, bool success, String message) {
    StaticJsonDocument<256> response;
    response["command_id"] = commandId;
    response["success"] = success;
    response["message"] = message;
    response["timestamp"] = millis() / 1000;
    
    String payload;
    serializeJson(response, payload);
    
    Serial.println("[CMD] Response: " + payload);

}