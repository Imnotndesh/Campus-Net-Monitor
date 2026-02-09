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
    static void handleDeepScan(String payload);
    static void handleConfigUpdate(String payload);
    static void handleRestart(String payload);
    static void handleOTAUpdate(String payload);
    static void handleFactoryReset(String payload);
    static void handlePing(String payload);
    static void handleGetStatus(String payload);
};

#endif