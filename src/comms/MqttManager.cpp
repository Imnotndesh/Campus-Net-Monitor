#include "MqttManager.h"
#include "CommandHandler.h"

WiFiClient MqttManager::espClient;
PubSubClient MqttManager::client(espClient);
String MqttManager::_probeId;
bool MqttManager::_deepScanTriggered = false;

void MqttManager::setup(const char* broker, int port, String probeId) {
    _probeId = probeId;
    client.setServer(broker, port);
    client.setCallback(callback);
    client.setBufferSize(1024);
}

void MqttManager::callback(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    
    // Convert payload to String
    char buffer[length + 1];
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    String message = String(buffer);
    
    Serial.printf("[MQTT] Message on topic: %s\n", topic);
    if (topicStr.indexOf("/cmd") != -1 || topicStr.indexOf("/command") != -1) {
        CommandHandler::processCommand(topicStr, message);
    } else if (topicStr.indexOf("/telemetry") != -1) {
        if ((char)payload[0] == '1') {
            _deepScanTriggered = true;
        }
    }
}

bool MqttManager::reconnect() {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    if (!client.connected()) {
        Serial.print("[MQTT] Attempting connection...");
        if (client.connect(_probeId.c_str())) {
            Serial.println("connected.");
            String cmdTopic = "campus/probes/" + _probeId + "/cmd";
            String broadcastTopic = "campus/probes/broadcast/cmd";
            
            client.subscribe(cmdTopic.c_str());
            client.subscribe(broadcastTopic.c_str());
            
            Serial.println("[MQTT] Subscribed to: " + cmdTopic);
            Serial.println("[MQTT] Subscribed to: " + broadcastTopic);
            
            syncOfflineLogs();
        } else {
            Serial.printf("failed, rc=%d\n", client.state());
        }
    }
    return client.connected();
}

void MqttManager::syncOfflineLogs() {
    if (StorageManager::getBufferSize() == 0) return;

    Serial.println("[MQTT] Draining local buffer to server...");
    String logs = StorageManager::readBuffer();
    
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

bool MqttManager::publishResponse(String topic, String payload) {
    if (client.connected()) {
        return client.publish(topic.c_str(), payload.c_str());
    }
    return false;
}

bool MqttManager::loop() {
    if (!client.connected()) reconnect();
    return client.loop();
}

bool MqttManager::isConnected(){
    return client.connected();
}

bool MqttManager::isDeepScanRequested() { 
    return _deepScanTriggered; 
}

void MqttManager::clearDeepScanFlag() { 
    _deepScanTriggered = false; 
}