#ifndef FLEET_MANAGER_H
#define FLEET_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../comms/MqttManager.h"
#include "../storage/ConfigManager.h"
#include "../packaging/JsonPackager.h"
#include "../packaging/TimeManager.h"
#include "FleetScheduler.h"
#include "FleetMembership.h"

class FleetManager {
public:
    static void begin();
    static void loop();
    
    static bool processFleetCommand(String command, JsonDocument& payload, String commandId);
    static void reportFleetStatus();
    static bool isWithinMaintenanceWindow();
    static void checkPendingOperations();

private:
    static bool initialized;
    static unsigned long lastStatusReport;
    static const unsigned long STATUS_INTERVAL = 300000;
    
    static void handleFleetConfig(JsonDocument& payload, String commandId);
    static void handleFleetGroups(JsonDocument& payload, String commandId);
    static void handleFleetLocation(JsonDocument& payload, String commandId);
    static void handleFleetTags(JsonDocument& payload, String commandId);
    static void handleFleetMaintenance(JsonDocument& payload, String commandId);
    static void handleFleetStatus(JsonDocument& payload, String commandId);
    static void handleFleetSchedule(JsonDocument& payload, String commandId);
    static void handleFleetOTA(JsonDocument& payload, String commandId);
    static void handleFleetDeepScan(JsonDocument& payload, String commandId);
    static void handleFleetReboot(JsonDocument& payload, String commandId);
    static void handleFleetFactoryReset(JsonDocument& payload, String commandId);
    
    static void subscribeToFleetTopics();
};

#endif