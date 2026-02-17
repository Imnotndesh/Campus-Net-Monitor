#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../storage/ConfigManager.h"
#include "../storage/StorageManager.h"
#include "../diagnostics/DiagnosticEngine.h"
#include "../firmware/OTAManager.h"
#include "../packaging/JsonPackager.h"
#include "MqttManager.h"

class CommandHandler {
public:
    static void begin();
    static void process(PendingCommand cmd);

private:
    static void handleDeepScan(PendingCommand cmd);
    static void handleConfigUpdate(PendingCommand cmd);
    static void handleRestart(PendingCommand cmd);
    static void handleOTAUpdate(PendingCommand cmd);
    static void handleFactoryReset(PendingCommand cmd);
    static void handlePing(PendingCommand cmd);
    static void handleGetStatus(PendingCommand cmd);
    static void handleGetConfig(PendingCommand cmd);
    static void handleSetWifi(PendingCommand cmd);
    static void handleSetMqtt(PendingCommand cmd);
    static void handleRenameProbe(PendingCommand cmd);
};

#endif