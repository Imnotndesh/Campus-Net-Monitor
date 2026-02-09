#include "MqttManager.h"

WiFiClient MqttManager::espClient;
PubSubClient MqttManager::client(espClient);
String MqttManager::_probeId;
bool MqttManager::_deepScanTriggered = false;
PendingCommand MqttManager::_currentCommand;

void MqttManager::setup(const char* broker, int port, String probeId) {
    _probeId = probeId;
    client.setServer(broker, port);
    client.setCallback(callback);
    client.setBufferSize(2048);
}

void MqttManager::callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.printf("[MQTT] Message on %s: %s\n", topic, message.c_str());

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, message);

    if (!error) {

        if (doc.containsKey("command")) {
            _currentCommand.type = doc["command"].as<String>();

            if (doc.containsKey("payload")) {
                String payloadStr;
                serializeJson(doc["payload"], payloadStr);
                _currentCommand.payload = payloadStr;
            } else {
                _currentCommand.payload = "{}";
            }
            
            _currentCommand.active = true;
            Serial.println("[MQTT] Command queued: " + _currentCommand.type);
        }
    } else {
        Serial.println("[MQTT] JSON Parse Error");
    }
}

bool MqttManager::reconnect() {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    if (!client.connected()) {
        Serial.print("[MQTT] Connecting...");
        String clientId = "ESP32-" + _probeId;
        
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");

            String cmdTopic = "campus/probes/" + _probeId + "/command";
            client.subscribe(cmdTopic.c_str());
            Serial.println("[MQTT] Subscribed: " + cmdTopic);
            
            syncOfflineLogs();
        } else {
            Serial.printf("failed, rc=%d\n", client.state());
        }
    }
    return client.connected();
}

bool MqttManager::hasPendingCommand() {
    return _currentCommand.active;
}

PendingCommand MqttManager::getNextCommand() {
    return _currentCommand;
}

void MqttManager::clearCommand() {
    _currentCommand.active = false;
    _currentCommand.type = "";
    _currentCommand.payload = "";
}

void MqttManager::publishCommandResult(String cmdType, String status, String resultJson) {
    String topic = "campus/probes/" + _probeId + "/result";
    
    DynamicJsonDocument doc(2048);
    doc["probe_id"] = _probeId;
    doc["command"] = cmdType;
    doc["status"] = status;

    DynamicJsonDocument resDoc(1024);
    deserializeJson(resDoc, resultJson);
    doc["result"] = resDoc;

    String output;
    serializeJson(doc, output);
    
    if (client.connected()) {
        client.publish(topic.c_str(), output.c_str());
        Serial.println("[MQTT] Result sent for " + cmdType);
    }
}
void MqttManager::syncOfflineLogs() {
    if (StorageManager::getBufferSize() == 0) return;
}

bool MqttManager::publishTelemetry(String payload) {
    if (client.connected()) {
        return client.publish("campus/probes/telemetry", payload.c_str());
    } else {
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