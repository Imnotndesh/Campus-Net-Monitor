#include "ResultBuffer.h"
DynamicJsonDocument ResultBuffer::buffer(16384);
bool ResultBuffer::initialized = false;

void ResultBuffer::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[RBUF]  Failed to mount LittleFS (Formatting...)");
        return;
    }
    
    loadBuffer();
    initialized = true;
    
    int count = getBufferCount();
    if (count > 0) {
        Serial.printf("[RBUF] Initialized with %d buffered results\n", count);
    }
}

bool ResultBuffer::saveResult(String cmdType, String status, String resultJson) {
    if (!initialized) {
        Serial.println("[RBUF]  Not initialized");
        return false;
    }
    
    loadBuffer();
    if (!buffer.containsKey("results")) {
        buffer.createNestedArray("results");
    }

    JsonArray results = buffer["results"].as<JsonArray>();
    if (results.size() >= MAX_BUFFERED_RESULTS) {
        Serial.println("[RBUF] ⚠ Buffer full, removing oldest result");
        results.remove(0); 
    }
    JsonObject newResult = results.createNestedObject();
    newResult["cmd"] = cmdType;
    newResult["status"] = status;
    newResult["timestamp"] = millis();
    DynamicJsonDocument tempDoc(4096);
    DeserializationError err = deserializeJson(tempDoc, resultJson);

    if (!err) {
        newResult["result"] = tempDoc;
    } else {
        newResult["result"] = resultJson;
    }
    
    return saveBuffer();
}

bool ResultBuffer::hasBufferedResults() {
    if (!initialized) return false;
    if (!buffer.containsKey("results")) return false;
    return buffer["results"].as<JsonArray>().size() > 0;
}

BufferedResult ResultBuffer::getNextResult() {
    BufferedResult res = {"", "", "","",0};
    if (!hasBufferedResults()) return res;
    
    JsonArray results = buffer["results"].as<JsonArray>();
    JsonObject obj = results[0];
    
    res.cmdType = obj["cmd"].as<String>();
    res.status = obj["status"].as<String>();
    res.timestamp = obj["timestamp"];

    String rawRes;
    serializeJson(obj["result"], rawRes);
    res.resultJson = rawRes;
    
    return res;
}

void ResultBuffer::clearResult() {
    if (!hasBufferedResults()) return;
    
    JsonArray results = buffer["results"].as<JsonArray>();
    results.remove(0);
    saveBuffer();
}

int ResultBuffer::getBufferCount() {
    if (!initialized) return 0;
    if (!buffer.containsKey("results")) return 0;
    return buffer["results"].as<JsonArray>().size();
}

void ResultBuffer::clearAll() {
    if (!initialized) return;
    
    buffer.clear();
    buffer.createNestedArray("results");
    saveBuffer();
    
    Serial.println("[RBUF] ✓ All buffered results cleared");
}

bool ResultBuffer::loadBuffer() {
    if (!LittleFS.exists(RESULT_BUFFER_FILE)) {
        buffer.clear();
        buffer.createNestedArray("results");
        return true;
    }
    
    File file = LittleFS.open(RESULT_BUFFER_FILE, "r");
    if (!file) {
        Serial.println("[RBUF] Failed to open buffer file");
        return false;
    }
    
    DeserializationError error = deserializeJson(buffer, file);
    file.close();
    
    if (error) {
        Serial.println("[RBUF] Buffer corrupted, resetting.");
        buffer.clear();
        buffer.createNestedArray("results");
        saveBuffer();
        return false;
    }
    
    return true;
}

bool ResultBuffer::saveBuffer() {
    File file = LittleFS.open(RESULT_BUFFER_FILE, "w");
    if (!file) {
        Serial.println("[RBUF]  Failed to open file for writing");
        return false;
    }
    
    if (serializeJson(buffer, file) == 0) {
        Serial.println("[RBUF]  Failed to write JSON to file");
        file.close();
        return false;
    }
    
    file.close();
    return true;
}