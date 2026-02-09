// src/comms/CommandHandler.h (Fixed)
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../storage/ConfigManager.h"
#include "../storage/StorageManager.h"
#include "../firmware/OTAManager.h"

enum CommandType {
    CMD_DEEP_SCAN,
    CMD_CONFIG_UPDATE,
    CMD_RESTART,
    CMD_OTA_UPDATE,
    CMD_FACTORY_RESET,
    CMD_PING,
    CMD_GET_STATUS,
    CMD_UNKNOWN
};

class CommandHandler {
public:
    static void begin();
    static void processCommand(String topic, String payload);
    static void sendCommandResponse(String commandId, bool success, String message);
    
private:
    static CommandType parseCommandType(String type);
    static void handleDeepScan(JsonObject& params);
    static void handleConfigUpdate(JsonObject& params);
    static void handleRestart(JsonObject& params);
    static void handleOTAUpdate(JsonObject& params);
    static void handleFactoryReset(JsonObject& params);
    static void handlePing(JsonObject& params);
    static void handleGetStatus(JsonObject& params);
};

#endif