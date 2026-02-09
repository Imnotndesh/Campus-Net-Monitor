#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../storage/StorageManager.h"

struct PendingCommand {
    String type;
    String payload;
    bool active = false;
};

class MqttManager {
public:
    static void setup(const char* broker, int port, String probeId);
    static bool loop();
    static bool publishTelemetry(String payload);
    static bool publishResponse(String topic, String payload);
    static void publishCommandResult(String cmdType, String status, String resultJson);
    static void syncOfflineLogs();
    static bool isConnected();
    static bool hasPendingCommand();
    static PendingCommand getNextCommand();
    static void clearCommand();
    static bool isDeepScanRequested();
    static void clearDeepScanFlag();

private:
    static void callback(char* topic, byte* payload, unsigned int length);
    static bool reconnect();
    
    static WiFiClient espClient;
    static PubSubClient client;
    static String _probeId;

    static PendingCommand _currentCommand;
    static bool _deepScanTriggered;
};

#endif