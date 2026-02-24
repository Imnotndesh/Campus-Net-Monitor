#include "FleetManager.h"
#include "../diagnostics/DiagnosticEngine.h"
#include "../firmware/OTAManager.h"

bool FleetManager::initialized = false;
unsigned long FleetManager::lastStatusReport = 0;

void FleetManager::begin() {
    if (initialized) return;
    
    Serial.println("[FLEET] Initializing Fleet Manager");
    
    FleetScheduler::begin();
    FleetMembership::begin();
    
    initialized = true;
    
    Serial.println("[FLEET] Fleet Manager ready");
}

void FleetManager::loop() {
    if (!initialized) return;
    
    FleetScheduler::checkSchedule();
    
    if (ConfigManager::isFleetManaged()) {
        unsigned long now = millis();
        if (now - lastStatusReport > STATUS_INTERVAL) {
            lastStatusReport = now;
            reportFleetStatus();
        }
    }
}

bool FleetManager::processFleetCommand(String command, JsonDocument& payload, String commandId) {
    if (!ConfigManager::isFleetManaged() && command != "fleet_enroll") {
        Serial.println("[FLEET] Not fleet managed, ignoring command");
        return false;
    }
    
    Serial.printf("[FLEET] Processing fleet command: %s (ID: %s)\n", command.c_str(), commandId.c_str());
    
    ConfigManager::incrementFleetCommandCount();
    ConfigManager::setLastFleetCommand(commandId);
    
    if (command == "fleet_config") {
        handleFleetConfig(payload, commandId);
    }
    else if (command == "fleet_groups") {
        handleFleetGroups(payload, commandId);
    }
    else if (command == "fleet_location") {
        handleFleetLocation(payload, commandId);
    }
    else if (command == "fleet_tags") {
        handleFleetTags(payload, commandId);
    }
    else if (command == "fleet_maintenance") {
        handleFleetMaintenance(payload, commandId);
    }
    else if (command == "fleet_status") {
        handleFleetStatus(payload, commandId);
    }
    else if (command == "fleet_schedule") {
        handleFleetSchedule(payload, commandId);
    }
    else if (command == "fleet_ota") {
        handleFleetOTA(payload, commandId);
    }
    else if (command == "fleet_deep_scan") {
        handleFleetDeepScan(payload, commandId);
    }
    else if (command == "fleet_reboot") {
        handleFleetReboot(payload, commandId);
    }
    else if (command == "fleet_factory_reset") {
        handleFleetFactoryReset(payload, commandId);
    }
    else if (command == "fleet_enroll") {
        ConfigManager::setFleetManaged(true);
        
        if (payload.containsKey("groups")) {
            handleFleetGroups(payload, commandId);
        }
        if (payload.containsKey("location")) {
            handleFleetLocation(payload, commandId);
        }
        if (payload.containsKey("tags")) {
            handleFleetTags(payload, commandId);
        }
        if (payload.containsKey("maintenance_window")) {
            handleFleetMaintenance(payload, commandId);
        }
        
        MqttManager::publishCommandResult("fleet_enroll", "completed", 
            "{\"msg\":\"Probe enrolled in fleet management\"}", commandId);
        
        reportFleetStatus();
    }
    else if (command == "fleet_unenroll") {
        ConfigManager::setFleetManaged(false);
        ConfigManager::clearFleetState();
        MqttManager::publishCommandResult("fleet_unenroll", "completed", 
            "{\"msg\":\"Probe removed from fleet management\"}", commandId);
    }
    else {
        Serial.printf("[FLEET] Unknown fleet command: %s\n", command.c_str());
        return false;
    }
    
    return true;
}

void FleetManager::handleFleetConfig(JsonDocument& payload, String commandId) {
    if (payload.containsKey("config")) {
        String configJson;
        serializeJson(payload["config"], configJson);
        
        if (ConfigManager::updateFromJSON(configJson)) {
            if (payload.containsKey("version")) {
                ConfigManager::setFleetConfigVersion(payload["version"]);
            }
            
            MqttManager::publishCommandResult("fleet_config", "completed", 
                "{\"msg\":\"Fleet config applied\"}", commandId);
        } else {
            MqttManager::publishCommandResult("fleet_config", "failed", 
                "{\"error\":\"Invalid config\"}", commandId);
        }
    }
}

void FleetManager::handleFleetGroups(JsonDocument& payload, String commandId) {
    if (payload.containsKey("groups")) {
        String groups;
        if (payload["groups"].is<JsonArray>()) {
            JsonArray arr = payload["groups"].as<JsonArray>();
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) groups += ",";
                groups += arr[i].as<String>();
            }
        } else {
            groups = payload["groups"].as<String>();
        }
        
        ConfigManager::setFleetGroups(groups);
        
        MqttManager::publishCommandResult("fleet_groups", "completed", 
            "{\"msg\":\"Groups updated\",\"groups\":\"" + groups + "\"}", commandId);
    }
}

void FleetManager::handleFleetLocation(JsonDocument& payload, String commandId) {
    if (payload.containsKey("location")) {
        String location = payload["location"].as<String>();
        ConfigManager::setFleetLocation(location);
        
        MqttManager::publishCommandResult("fleet_location", "completed", 
            "{\"msg\":\"Location updated\",\"location\":\"" + location + "\"}", commandId);
    }
}

void FleetManager::handleFleetTags(JsonDocument& payload, String commandId) {
    if (payload.containsKey("tags")) {
        String tags;
        serializeJson(payload["tags"], tags);
        ConfigManager::setFleetTags(tags);
        
        MqttManager::publishCommandResult("fleet_tags", "completed", 
            "{\"msg\":\"Tags updated\"}", commandId);
    }
}

void FleetManager::handleFleetMaintenance(JsonDocument& payload, String commandId) {
    if (payload.containsKey("window")) {
        String window = payload["window"].as<String>();
        ConfigManager::setMaintenanceWindow(window);
        
        MqttManager::publishCommandResult("fleet_maintenance", "completed", 
            "{\"msg\":\"Maintenance window set\",\"window\":\"" + window + "\"}", commandId);
    }
}

void FleetManager::handleFleetStatus(JsonDocument& payload, String commandId) {
    reportFleetStatus();
    MqttManager::publishCommandResult("fleet_status", "completed", 
        "{\"msg\":\"Status report sent\"}", commandId);
}

void FleetManager::handleFleetSchedule(JsonDocument& payload, String commandId) {
    if (payload.containsKey("operation") && payload.containsKey("schedule")) {
        String operation = payload["operation"].as<String>();
        // Pass the nested JsonObject directly, not re-serialized
        JsonVariant scheduleVar = payload["schedule"];
        DynamicJsonDocument scheduleDoc(512);
        scheduleDoc.set(scheduleVar);
        
        if (FleetScheduler::scheduleOperation(operation, scheduleDoc)) {
            MqttManager::publishCommandResult("fleet_schedule", "completed",
                "{\"msg\":\"Operation scheduled\"}", commandId);
        } else {
            MqttManager::publishCommandResult("fleet_schedule", "failed",
                "{\"error\":\"Invalid schedule\"}", commandId);
        }
    }
}

void FleetManager::handleFleetOTA(JsonDocument& payload, String commandId) {
    if (payload.containsKey("url")) {
        String url = payload["url"].as<String>();
        String version = payload["version"] | "";
        
        if (version.length() > 0) {
            ConfigManager::setFirmwareVersion(version);
        }
        
        MqttManager::publishCommandResult("fleet_ota", "processing", 
            "{\"msg\":\"OTA initiated\"}", commandId);
        
        OTAManager::performUpdate(url.c_str(), commandId.c_str());
    }
}

void FleetManager::handleFleetDeepScan(JsonDocument& payload, String commandId) {
    MqttManager::publishCommandResult("fleet_deep_scan", "processing", 
        "{\"msg\":\"Deep scan initiated\"}", commandId);
    
    int duration = payload["duration"] | 5;
    String target = payload["target"] | "8.8.8.8";
    
    EnhancedMetrics em = DiagnosticEngine::performDeepAnalysis(target.c_str());
    String probeId = String(ConfigManager::getProbeId());
    String resultPayload = JsonPackager::serializeEnhanced(em, probeId);
    
    MqttManager::publishCommandResult("fleet_deep_scan", "completed", 
        resultPayload, commandId);
    
    WifiCredentials creds = StorageManager::loadWifiCredentials();
    WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
}

void FleetManager::handleFleetReboot(JsonDocument& payload, String commandId) {
    int delayMs = payload["delay"] | 2000;
    
    MqttManager::publishCommandResult("fleet_reboot", "completed", 
        "{\"msg\":\"Rebooting...\"}", commandId);
    
    delay(delayMs);
    ESP.restart();
}

void FleetManager::handleFleetFactoryReset(JsonDocument& payload, String commandId) {
    MqttManager::publishCommandResult("fleet_factory_reset", "processing", 
        "{\"msg\":\"Wiping data...\"}", commandId);
    
    StorageManager::wipe();
    ConfigManager::clearFleetState();
    
    delay(1000);
    ESP.restart();
}

void FleetManager::reportFleetStatus() {
    if (!ConfigManager::isFleetManaged()) return;
    
    DynamicJsonDocument doc(1024);
    
    doc["probe_id"] = ConfigManager::getProbeId();
    doc["firmware"] = ConfigManager::getFirmwareVersion();
    doc["uptime"] = millis() / 1000;
    doc["timestamp"] = TimeManager::getTimestamp();
    doc["epoch"] = TimeManager::getEpoch();
    
    doc["managed"] = true;
    doc["groups"] = ConfigManager::getFleetGroups();
    doc["location"] = ConfigManager::getFleetLocation();
    doc["config_version"] = ConfigManager::getFleetConfigVersion();
    doc["commands_processed"] = ConfigManager::getFleetCommandCount();
    doc["last_command"] = ConfigManager::getLastFleetCommand();
    
    String tagsStr = ConfigManager::getFleetTags();
    if (tagsStr.length() > 0 && tagsStr != "{}") {
        DynamicJsonDocument tagsDoc(512);
        deserializeJson(tagsDoc, tagsStr);
        doc["tags"] = tagsDoc;
    }
    
    String maintWindow = ConfigManager::getMaintenanceWindow();
    if (maintWindow.length() > 0) {
        doc["maintenance_window"] = maintWindow;
        doc["in_maintenance"] = isWithinMaintenanceWindow();
    }
    
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["mqtt_connected"] = MqttManager::isConnected();
    doc["free_heap"] = ESP.getFreeHeap();
    
    String payload;
    serializeJson(doc, payload);
    
    MqttManager::publishBroadcast("campus/fleet/status/" + String(ConfigManager::getProbeId()), payload);
}

bool FleetManager::isWithinMaintenanceWindow() {
    String window = ConfigManager::getMaintenanceWindow();
    if (window.length() == 0) return true;
    
    int separator = window.indexOf('-');
    if (separator == -1) return true;
    
    String startStr = window.substring(0, separator);
    String endStr = window.substring(separator + 1);
    
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    
    int currentMinutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    
    int startColon = startStr.indexOf(':');
    if (startColon == -1) return true;
    int startHour = startStr.substring(0, startColon).toInt();
    int startMin = startStr.substring(startColon + 1).toInt();
    int startMinutes = startHour * 60 + startMin;
    
    int endColon = endStr.indexOf(':');
    if (endColon == -1) return true;
    int endHour = endStr.substring(0, endColon).toInt();
    int endMin = endStr.substring(endColon + 1).toInt();
    int endMinutes = endHour * 60 + endMin;
    
    if (endMinutes < startMinutes) {
        return (currentMinutes >= startMinutes || currentMinutes < endMinutes);
    } else {
        return (currentMinutes >= startMinutes && currentMinutes < endMinutes);
    }
}