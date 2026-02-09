#include "MqttManager.h"

WiFiClient MqttManager::espClient;
PubSubClient MqttManager::client(espClient);
String MqttManager::_probeId;
PendingCommand MqttManager::_currentCommand;

void MqttManager::setup(const char* broker, int port, String probeId) {
    _probeId = probeId;
    client.setServer(broker, port);
    client.setCallback(callback);
    // Increase buffer size to handle large JSON commands and results
    client.setBufferSize(4096); 
}

void MqttManager::callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.printf("[MQTT] Message received on %s\n", topic);

    // Parse JSON Command from API
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, message);

    if (!error) {
        if (doc.containsKey("command")) {
            _currentCommand.type = doc["command"].as<String>();
            
            // Extract payload if it exists
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
        Serial.print("[MQTT] JSON Parse Error: ");
        Serial.println(error.c_str());
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
    
    DynamicJsonDocument doc(4096);
    doc["probe_id"] = _probeId;
    doc["command"] = cmdType;
    doc["status"] = status;
    if (resultJson.length() > 0) {
        doc["result"] = serialized(resultJson.c_str());
    } else {
        doc["result"] = serialized("{}");
    }

    String output;
    serializeJson(doc, output);
    
    if (client.connected()) {
        if (client.publish(topic.c_str(), output.c_str())) {
             Serial.println("[MQTT] Result sent for " + cmdType);
        } else {
             Serial.println("[MQTT] Failed to send result (Packet too big?)");
        }
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

bool MqttManager::loop() {
    if (!client.connected()) reconnect();
    return client.loop();
}

bool MqttManager::isConnected(){
    return client.connected();
}