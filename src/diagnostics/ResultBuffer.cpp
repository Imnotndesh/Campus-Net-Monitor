#include "ResultBuffer.h"

DynamicJsonDocument ResultBuffer::buffer(8192);
bool ResultBuffer::initialized = false;

void ResultBuffer::begin() {
    // Fixed: Use LittleFS with format-on-fail enabled
    if (!LittleFS.begin(true)) {
        Serial.println("[RBUF]  Failed to mount LittleFS (Formatting...)");
        return;
    }
    
    loadBuffer();
    initialized = true;
    Serial.printf("[RBUF] Initialized with %d buffered results\n", getBufferCount());
}

bool ResultBuffer::saveResult(String cmdType, String status, String resultJson) {
    if (!initialized) {
        Serial.println("[RBUF]  Not initialized");
        return false;
    }
    
    // Ensure we have the latest state
    loadBuffer();
    
    // Initialize array if missing
    if (!buffer.containsKey("results")) {
        buffer.createNestedArray("results");
    }

    JsonArray results = buffer["results"].as<JsonArray>();
    
    // Cap buffer size (FIFO)
    if (results.size() >= MAX_BUFFERED_RESULTS) {
        Serial.println("[RBUF] ⚠ Buffer full, removing oldest result");
        results.remove(0); 
    }
    
    // Add new result
    JsonObject newResult = results.createNestedObject();
    newResult["cmd"] = cmdType;
    newResult["status"] = status;
    
    // Use serialized to prevent double-escaping
    newResult["result"] = serialized(resultJson.c_str());
    newResult["timestamp"] = millis();
    
    // Write to disk
    return saveBuffer();
}

bool ResultBuffer::hasBufferedResults() {
    if (!initialized) return false;
    // Don't reload every time to avoid flash wear, rely on memory state
    if (!buffer.containsKey("results")) return false;
    return buffer["results"].as<JsonArray>().size() > 0;
}

BufferedResult ResultBuffer::getNextResult() {
    BufferedResult res = {"", "", "", 0};
    if (!hasBufferedResults()) return res;
    
    JsonArray results = buffer["results"].as<JsonArray>();
    JsonObject obj = results[0];
    
    res.cmdType = obj["cmd"].as<String>();
    res.status = obj["status"].as<String>();
    
    // Handle serialized data correctly
    String rawRes;
    serializeJson(obj["result"], rawRes);
    res.resultJson = rawRes;
    
    res.timestamp = obj["timestamp"];
    
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
    
    Serial.println("[RBUF]  All buffered results cleared");
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
        // Save clean state immediately to fix corruption
        saveBuffer();
        return false;
    }
    
    return true;
}

bool ResultBuffer::saveBuffer() {
    // Open with "w" to truncate and overwrite
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