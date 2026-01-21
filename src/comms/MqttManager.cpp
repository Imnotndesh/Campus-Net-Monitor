#include "MqttManager.h"

WiFiClient MqttManager::espClient;
PubSubClient MqttManager::client(espClient);
String MqttManager::_probeId;
bool MqttManager::_deepScanTriggered = false;

void MqttManager::setup(const char* broker, int port, String probeId) {
    _probeId = probeId;
    client.setServer(broker, port);
    client.setCallback(callback);
}

void MqttManager::callback(char* topic, byte* payload, unsigned int length) {
    if (String(topic).endsWith("/cmd")) {
        if ((char)payload[0] == '1') _deepScanTriggered = true;
    }
}

bool MqttManager::reconnect() {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    if (!client.connected()) {
        Serial.print("[MQTT] Attempting connection...");
        if (client.connect(_probeId.c_str())) {
            Serial.println("connected.");
            client.subscribe(("campus/probes/" + _probeId + "/cmd").c_str());
            syncOfflineLogs(); // Drain LittleFS on reconnection
        }
    }
    return client.connected();
}

void MqttManager::syncOfflineLogs() {
    if (StorageManager::getBufferSize() == 0) return;

    Serial.println("[MQTT] Draining local buffer to server...");
    String logs = StorageManager::readBuffer();
    
    // Split the buffer by newlines and publish each record
    int start = 0;
    int end = logs.indexOf('\n');
    while (end != -1) {
        String record = logs.substring(start, end);
        client.publish("campus/probes/telemetry/offline", record.c_str());
        start = end + 1;
        end = logs.indexOf('\n', start);
    }
    StorageManager::clearBuffer();
    Serial.println("[MQTT] Buffer cleared.");
}

bool MqttManager::publishTelemetry(String payload) {
    if (client.connected()) {
        return client.publish("campus/probes/telemetry", payload.c_str());
    } else {
        Serial.println("[ERROR] MQTT Offline. Redirecting to LittleFS.");
        return StorageManager::appendToBuffer(payload);
    }
}

bool MqttManager::loop() {
    if (!client.connected()) reconnect();
    return client.loop();
}

bool MqttManager::isDeepScanRequested() { return _deepScanTriggered; }
void MqttManager::clearDeepScanFlag() { _deepScanTriggered = false; }