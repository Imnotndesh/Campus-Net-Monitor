#include "MqttManager.h"

WiFiClient MqttManager::espClient;
PubSubClient MqttManager::client(espClient);
String MqttManager::_probeId;
PendingCommand MqttManager::_currentCommand;

void MqttManager::setup(const char* broker, int port, String probeId) {
    _probeId = probeId;
    client.setServer(broker, port);
    client.setCallback(callback);
    client.setBufferSize(4096);
    
    // Initialize result buffer
    ResultBuffer::begin();
}

void MqttManager::callback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("[MQTT] ║ MESSAGE RECEIVED\n");
    
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.printf("[MQTT] ║ JSON Parse Error: %s\n", error.c_str());
        return;
    }
    
    for (JsonPair kv : doc.as<JsonObject>()) {
        Serial.printf("%s ", kv.key().c_str());
    }
    Serial.println();
    
    if (doc.containsKey("command")) {
        _currentCommand.type = doc["command"].as<String>();
        _currentCommand.id = doc["command_id"].as<String>();
        Serial.printf("[MQTT] ║ Command Type: %s\n", _currentCommand.type.c_str());
        Serial.printf("[MQTT] ║ Command ID: %s\n", _currentCommand.id.c_str());
        
        if (doc.containsKey("payload")) {
            String payloadStr;
            serializeJson(doc["payload"], payloadStr);
            _currentCommand.payload = payloadStr;
            Serial.printf("[MQTT] ║ Payload: %s\n", payloadStr.c_str());
        } else {
            _currentCommand.payload = "{}";
            Serial.println("[MQTT] ║ No payload field, using empty object");
        }
        
        _currentCommand.active = true;
    } else {
        Serial.println("[MQTT] ║ No 'command' field in JSON!");
    }
}

bool MqttManager::reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MQTT] WiFi not connected, cannot connect to MQTT");
        return false;
    }
    
    if (!client.connected()) {
        Serial.print("[MQTT] Attempting connection...");
        String clientId = "ESP32-" + _probeId;
        
        if (client.connect(clientId.c_str())) {
            Serial.println(" CONNECTED");
            Serial.printf("[MQTT] Client ID: %s\n", clientId.c_str());
            
            String cmdTopic = "campus/probes/" + _probeId + "/command";
            bool subResult = client.subscribe(cmdTopic.c_str());
            
            if (subResult) {
                Serial.println("[MQTT] ✓ SUBSCRIBED TO: " + cmdTopic);
            } else {
                Serial.println("[MQTT]  SUBSCRIPTION FAILED: " + cmdTopic);
            }
            
            // Sync offline data
            syncOfflineLogs();
            syncBufferedResults();  // NEW: Sync buffered command results
            
        } else {
            Serial.printf(" FAILED, rc=%d\n", client.state());
            Serial.print("[MQTT] Error meaning: ");
            switch(client.state()) {
                case -4: Serial.println("Connection timeout"); break;
                case -3: Serial.println("Connection lost"); break;
                case -2: Serial.println("Connect failed"); break;
                case -1: Serial.println("Disconnected"); break;
                case  1: Serial.println("Bad protocol"); break;
                case  2: Serial.println("Client ID rejected"); break;
                case  3: Serial.println("Server unavailable"); break;
                case  4: Serial.println("Bad credentials"); break;
                case  5: Serial.println("Unauthorized"); break;
                default: Serial.println("Unknown error"); break;
            }
        }
    }
    return client.connected();
}

void MqttManager::publishCommandResult(String cmdType, String status, String resultPayload, String cmdId) {
    Serial.printf("[MQTT] Publishing result: cmd=%s, status=%s, id=%s\n", cmdType.c_str(), status.c_str(), cmdId.c_str());
    
    if (client.connected()) {
        if (publishResultInternal(cmdType, status, resultPayload, cmdId)) {
            Serial.println("[MQTT] ✓ Result published immediately");
            return;
        }
    }
    
    Serial.println("[MQTT] ⚠ Not connected or publish failed, buffering to disk");
    if (ResultBuffer::saveResult(cmdType, status, resultPayload)) {
        Serial.println("[MQTT] ✓ Result buffered to disk for later sync");
    } else {
        Serial.println("[MQTT] ✗ Failed to buffer result!");
    }
}

bool MqttManager::publishResultInternal(String cmdType, String status, String resultJson,String cmdId) {
    String topic = "campus/probes/" + _probeId + "/result";
    
    DynamicJsonDocument doc(4096);
    doc["probe_id"] = _probeId;
    doc["command"] = cmdType;
    doc["status"] = status;
    doc["command_id"] = cmdId;
    
    if (resultJson.length() > 0) {
        doc["result"] = serialized(resultJson.c_str());
    } else {
        doc["result"] = serialized("{}");
    }

    String output;
    serializeJson(doc, output);
    
    Serial.printf("[MQTT] Result size: %d bytes\n", output.length());
    
    if (output.length() > 4096) {
        Serial.println("[MQTT] ⚠ Result exceeds buffer size!");
        return false;
    }
    
    bool success = client.publish(topic.c_str(), output.c_str());
    
    if (success) {
        Serial.printf("[MQTT] Published to: %s\n", topic.c_str());
    } else {
        Serial.println("[MQTT] Publish failed");
    }
    
    return success;
}

void MqttManager::syncBufferedResults() {
    if (!ResultBuffer::hasBufferedResults()) {
        return;
    }
    
    int count = ResultBuffer::getBufferCount();
    Serial.printf("[MQTT] ║ SYNCING %d BUFFERED RESULTS\n", count);
    
    int synced = 0;
    int failed = 0;
    
    while (ResultBuffer::hasBufferedResults() && synced < 5) {
        BufferedResult result = ResultBuffer::getNextResult();
        
        Serial.printf("[MQTT] ║ Syncing: %s (status: %s)\n", 
                      result.cmdType.c_str(), result.status.c_str());
        
        if (publishResultInternal(result.cmdType, result.status, result.resultJson,result.cmdId)) {
            ResultBuffer::clearResult();
            synced++;
            Serial.println("[MQTT] ║   ✓ Synced successfully");
        } else {
            failed++;
            Serial.println("[MQTT] ║  Sync failed, will retry later");
            break; // Stop trying if one fails
        }
        
        delay(100); // Small delay between publishes
    }
    
    Serial.printf("[MQTT] ║ Synced: %d, Failed: %d, Remaining: %d\n", synced, failed, ResultBuffer::getBufferCount());
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

void MqttManager::syncOfflineLogs() {
    if (StorageManager::getBufferSize() == 0) {
        Serial.println("[MQTT] No offline logs to sync");
        return;
    }
    
    Serial.println("[MQTT] Syncing offline logs...");
    
    String buffer = StorageManager::readBuffer();
    if (buffer.length() == 0) {
        Serial.println("[MQTT] Buffer empty after read");
        StorageManager::clearBuffer();
        return;
    }
    
    int synced = 0;
    int failed = 0;
    int start = 0;
    
    while (start < buffer.length()) {
        int end = buffer.indexOf('\n', start);
        if (end == -1) end = buffer.length();
        
        String line = buffer.substring(start, end);
        line.trim();
        
        if (line.length() > 2) {
            if (client.publish("campus/probes/telemetry", line.c_str())) {
                synced++;
            } else {
                failed++;
            }
            delay(50);
        }
        
        start = end + 1;
    }
    
    StorageManager::clearBuffer();
    Serial.printf("[MQTT] Offline sync complete: %d synced, %d failed\n", synced, failed);
}

bool MqttManager::publishTelemetry(String payload) {
    if (client.connected()) {
        return client.publish("campus/probes/telemetry", payload.c_str());
    } else {
        return StorageManager::appendToBuffer(payload);
    }
}

bool MqttManager::loop() {
    if (!client.connected()) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            reconnect();
        }
    }
    return client.loop();
}

bool MqttManager::isConnected(){
    return client.connected();
}