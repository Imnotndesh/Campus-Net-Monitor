#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "../storage/StorageManager.h"

class MqttManager {
public:
    static void setup(const char* broker, int port, String probeId);
    static bool loop(); // Returns true if connected
    static bool publishTelemetry(String payload);
    static void syncOfflineLogs();
    static bool isDeepScanRequested();
    static void clearDeepScanFlag();
    static bool isConnected();

private:
    static void callback(char* topic, byte* payload, unsigned int length);
    static bool reconnect();
    static WiFiClient espClient;
    static PubSubClient client;
    static String _probeId;
    static bool _deepScanTriggered;
};

#endif