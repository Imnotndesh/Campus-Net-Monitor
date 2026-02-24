#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "../storage/StorageManager.h"
#include "../diagnostics/ResultBuffer.h"

struct PendingCommand {
    String type;
    String payload;
    String id;
    bool active = false;
};

class MqttManager {
public:
    static void setup(const char* broker, int port, String probeId);
    static bool loop();
    static bool publishTelemetry(String payload);
    
    static void publishCommandResult(String cmdType, String status, String resultPayload, String cmdId);  
    static bool publishBroadcast(String topic, String payload);

    static bool hasPendingCommand();
    static PendingCommand getNextCommand();
    static void clearCommand();
    static void syncOfflineLogs();
    static void syncBufferedResults();
    
    static bool isConnected();
    static void subscribeToFleetTopics();
    static bool isFleetTopic(String topic);

private:
    static void callback(char* topic, byte* payload, unsigned int length);
    static bool reconnect();
    static bool publishResultInternal(String cmdType, String status, String resultJson, String cmdId);
    
    static WiFiClient espClient;
    static PubSubClient client;
    static String _probeId;
    
    static PendingCommand _currentCommand;
};

#endif