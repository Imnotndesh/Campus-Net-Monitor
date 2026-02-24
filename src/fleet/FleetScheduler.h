#ifndef FLEET_SCHEDULER_H
#define FLEET_SCHEDULER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "../packaging/TimeManager.h"

struct ScheduledOperation {
    String id;
    String type;
    time_t executeAt;
    unsigned long executeAtMillis;
    String parameters;
    bool recurring;
    String cronPattern;
    bool executed;
    ScheduledOperation() : executeAt(0), executeAtMillis(0), recurring(false), executed(false) {}
};

class FleetScheduler {
public:
    static void begin();
    static void checkSchedule();
    
    static bool scheduleOperation(String type, JsonDocument& schedule);
    static bool cancelOperation(String id);
    static std::vector<ScheduledOperation> getPendingOperations();
    static String getSchedulesJson();

private:
    static std::vector<ScheduledOperation> operations;
    static bool initialized;
    
    static void loadSchedules();
    static void saveSchedules();
    static void executeOperation(const ScheduledOperation& op);
    static time_t parseScheduleTime(JsonDocument& schedule);
    static unsigned long parseRelativeTime(JsonDocument& schedule);
    static JsonDocument parseParameters(const String& params);
};

#endif