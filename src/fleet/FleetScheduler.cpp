#include "FleetScheduler.h"
#include "../storage/StorageManager.h"
#include "../storage/ConfigManager.h"
#include "../comms/MqttManager.h"
#include "../diagnostics/DiagnosticEngine.h"
#include "../packaging/JsonPackager.h"

std::vector<ScheduledOperation> FleetScheduler::operations;
bool FleetScheduler::initialized = false;

void FleetScheduler::begin() {
    if (initialized) return;
    
    loadSchedules();
    initialized = true;
    
    Serial.printf("[FLEET] Scheduler initialized with %d operations\n", operations.size());
}

void FleetScheduler::checkSchedule() {
    if (!initialized) return;
    
    time_t now = time(nullptr);
    unsigned long nowMillis = millis();
    bool changed = false;
    
    for (auto& op : operations) {
        if (op.executed) continue;
        
        bool shouldExecute = false;
        
        if (op.executeAt > 0) {
            shouldExecute = (now >= op.executeAt);
        } else if (op.executeAtMillis > 0) {
            shouldExecute = (nowMillis >= op.executeAtMillis);
        }
        
        if (shouldExecute) {
            executeOperation(op);
            op.executed = true;
            changed = true;
            
            if (op.recurring) {
                if (op.cronPattern.length() > 0) {
                    if (op.cronPattern == "@daily") {
                        op.executeAt += 86400;
                    } else if (op.cronPattern == "@hourly") {
                        op.executeAt += 3600;
                    } else if (op.cronPattern == "@weekly") {
                        op.executeAt += 604800;
                    }
                } else {
                    op.executeAt += 86400;
                }
                op.executed = false;
            }
        }
    }
    
    if (changed) {
        saveSchedules();
    }
}

bool FleetScheduler::scheduleOperation(String type, JsonDocument& schedule) {
    ScheduledOperation op;
    op.id = String(millis(), HEX) + String(random(1000, 9999));
    op.type = type;
    op.executed = false;
    
    if (schedule.containsKey("at")) {
        op.executeAt = parseScheduleTime(schedule);
        op.executeAtMillis = 0;
    } else if (schedule.containsKey("in")) {
        op.executeAtMillis = parseRelativeTime(schedule);
        op.executeAt = 0;
    } else {
        return false;
    }
    
    op.recurring = schedule.containsKey("recurring") ? schedule["recurring"].as<bool>() : false;
    op.cronPattern = schedule["cron"] | "";
    
    if (schedule.containsKey("parameters")) {
        // Serialize parameters to string
        String paramsStr;
        serializeJson(schedule["parameters"], paramsStr);
        op.parameters = paramsStr;
    }
    
    operations.push_back(op);
    saveSchedules();
    
    return true;
}

bool FleetScheduler::cancelOperation(String id) {
    for (auto it = operations.begin(); it != operations.end(); ++it) {
        if (it->id == id) {
            operations.erase(it);
            saveSchedules();
            return true;
        }
    }
    return false;
}

std::vector<ScheduledOperation> FleetScheduler::getPendingOperations() {
    std::vector<ScheduledOperation> pending;
    for (const auto& op : operations) {
        if (!op.executed) {
            pending.push_back(op);
        }
    }
    return pending;
}

String FleetScheduler::getSchedulesJson() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& op : operations) {
        if (!op.executed || op.recurring) {
            JsonObject obj = arr.createNestedObject();
            obj["id"] = op.id;
            obj["type"] = op.type;
            obj["execute_at"] = op.executeAt;
            obj["recurring"] = op.recurring;
            obj["cron"] = op.cronPattern;
        }
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void FleetScheduler::executeOperation(const ScheduledOperation& op) {
    Serial.printf("[FLEET] Executing scheduled operation: %s\n", op.type.c_str());
    
    DynamicJsonDocument result(1024);
    result["scheduled_id"] = op.id;
    result["type"] = op.type;
    result["timestamp"] = TimeManager::getTimestamp();
    result["epoch"] = TimeManager::getEpoch();
    
    if (op.type == "deep_scan") {
        // Parse parameters
        DynamicJsonDocument params(512);
        if (op.parameters.length() > 0) {
            deserializeJson(params, op.parameters);
        }
        
        String target = params["target"] | "8.8.8.8";
        EnhancedMetrics em = DiagnosticEngine::performDeepAnalysis(target.c_str());
        String probeId = String(ConfigManager::getProbeId());
        String payload = JsonPackager::serializeEnhanced(em, probeId);
        
        MqttManager::publishBroadcast("campus/fleet/scheduled/deep_scan", payload);
        
        WifiCredentials creds = StorageManager::loadWifiCredentials();
        WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
    }
    else if (op.type == "shutdown") {
        result["msg"] = "Entering deep sleep";
        String payload;
        serializeJson(result, payload);
        
        MqttManager::publishBroadcast("campus/fleet/scheduled/shutdown", payload);
        delay(1000);
        esp_deep_sleep_start();
    }
    else if (op.type == "reboot") {
        result["msg"] = "Rebooting";
        String payload;
        serializeJson(result, payload);
        
        MqttManager::publishBroadcast("campus/fleet/scheduled/reboot", payload);
        delay(1000);
        ESP.restart();
    }
    else if (op.type == "report_status") {
        DynamicJsonDocument status(256);
        status["probe_id"] = ConfigManager::getProbeId();
        status["uptime"] = millis() / 1000;
        status["rssi"] = WiFi.RSSI();
        status["timestamp"] = TimeManager::getTimestamp();
        
        String payload;
        serializeJson(status, payload);
        MqttManager::publishBroadcast("campus/fleet/scheduled/status", payload);
    }
    else if (op.type == "light_scan") {
        NetworkMetrics m = DiagnosticEngine::performFullTest("8.8.8.8");
        String probeId = String(ConfigManager::getProbeId());
        String payload = JsonPackager::serializeLight(m, probeId);
        
        MqttManager::publishBroadcast("campus/fleet/scheduled/light_scan", payload);
    }
}

time_t FleetScheduler::parseScheduleTime(JsonDocument& schedule) {
    if (schedule["at"].is<unsigned long>()) {
        return schedule["at"].as<time_t>();
    }
    
    String timeStr = schedule["at"].as<String>();
    
    if (timeStr.startsWith("+")) {
        int seconds = timeStr.substring(1).toInt();
        return time(nullptr) + seconds;
    }
    
    struct tm tm = {0};
    if (strptime(timeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm) != NULL) {
        return mktime(&tm);
    }
    
    return time(nullptr) + 3600;
}

unsigned long FleetScheduler::parseRelativeTime(JsonDocument& schedule) {
    if (schedule["in"].is<unsigned long>()) {
        return millis() + schedule["in"].as<unsigned long>();
    }
    
    String timeStr = schedule["in"].as<String>();
    char unit = timeStr.charAt(timeStr.length() - 1);
    int value = timeStr.substring(0, timeStr.length() - 1).toInt();
    
    switch(unit) {
        case 's': return millis() + (value * 1000);
        case 'm': return millis() + (value * 60 * 1000);
        case 'h': return millis() + (value * 60 * 60 * 1000);
        case 'd': return millis() + (value * 24 * 60 * 60 * 1000);
        default: return millis() + (value * 1000);
    }
}

void FleetScheduler::loadSchedules() {
    File file = LittleFS.open("/schedules.json", "r");
    if (!file) return;
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) return;
    
    operations.clear();
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        ScheduledOperation op;
        op.id = obj["id"].as<String>();
        op.type = obj["type"].as<String>();
        op.executeAt = obj["execute_at"];
        op.executeAtMillis = obj["execute_at_ms"];
        op.recurring = obj["recurring"];
        op.cronPattern = obj["cron"].as<String>();
        op.executed = obj["executed"];
        
        if (obj.containsKey("params")) {
            String paramsStr;
            serializeJson(obj["params"], paramsStr);
            op.parameters = paramsStr;
        }
        
        operations.push_back(op);
    }
}

void FleetScheduler::saveSchedules() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    
    for (const auto& op : operations) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = op.id;
        obj["type"] = op.type;
        obj["execute_at"] = op.executeAt;
        obj["execute_at_ms"] = op.executeAtMillis;
        obj["recurring"] = op.recurring;
        obj["cron"] = op.cronPattern;
        obj["executed"] = op.executed;
        
        if (op.parameters.length() > 0) {
            DynamicJsonDocument paramsDoc(512);
            deserializeJson(paramsDoc, op.parameters);
            obj["params"] = paramsDoc;
        }
    }
    
    File file = LittleFS.open("/schedules.json", "w");
    if (!file) return;
    
    serializeJson(doc, file);
    file.close();
}