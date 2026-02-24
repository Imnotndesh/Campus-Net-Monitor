// OTAManager.cpp - Enhanced version
#include "OTAManager.h"

void (*OTAManager::progressCallback)(int, int, const char*) = nullptr;
void (*OTAManager::errorCallback)(int, const char*) = nullptr;
void (*OTAManager::startCallback)(const char*) = nullptr;
void (*OTAManager::finishCallback)(const char*) = nullptr;
String OTAManager::currentCmdId = "";

void OTAManager::setProgressCallback(void (*callback)(int, int, const char*)) {
    progressCallback = callback;
}

void OTAManager::setErrorCallback(void (*callback)(int, const char*)) {
    errorCallback = callback;
}

void OTAManager::setStartCallback(void (*callback)(const char*)) {
    startCallback = callback;
}

void OTAManager::setFinishCallback(void (*callback)(const char*)) {
    finishCallback = callback;
}

bool OTAManager::performUpdate(const char* url, const char* cmdId) {
    currentCmdId = String(cmdId);
    
    // Pre-update checks
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] WiFi not connected");
        if (errorCallback) errorCallback(-1, currentCmdId.c_str());
        return false;
    }

    Serial.printf("[OTA] Starting update from URL: %s\n", url);
    if (startCallback) startCallback(currentCmdId.c_str());
    
    HTTPClient http;
    http.setTimeout(30000); // 30 second timeout for slow connections
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Follow redirects (common for CDN links)
    
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] HTTP Failed: %d\n", httpCode);
        if (errorCallback) {
            String errorMsg = "HTTP error: " + String(httpCode);
            errorCallback(httpCode, currentCmdId.c_str());
        }
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("[OTA] Invalid content length");
        if (errorCallback) errorCallback(-2, currentCmdId.c_str());
        http.end();
        return false;
    }

    if (!Update.begin(contentLength)) {
        Serial.printf("[OTA] Not enough space. Need: %d, Available: %d\n", 
                     contentLength, ESP.getFreeSketchSpace());
        if (errorCallback) errorCallback(-3, currentCmdId.c_str());
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buff[256];
    int lastProgress = -1;
    unsigned long lastReport = 0;

    while (http.connected() && written < contentLength) {
        size_t size = stream->available();
        if (size) {
            int bytesRead = stream->readBytes(buff, min(size, sizeof(buff)));
            if (bytesRead > 0) {
                Update.write(buff, bytesRead);
                written += bytesRead;

                int progress = (written * 100) / contentLength;
                
                unsigned long now = millis();
                if (progress != lastProgress && progressCallback && (now - lastReport > 500)) {
                    progressCallback(written, contentLength, currentCmdId.c_str());
                    lastProgress = progress;
                    lastReport = now;
                }
            }
        }
        delay(1);

        if (millis() - lastReport > 30000) {
            Serial.println("[OTA] Update timeout - no data received");
            Update.abort();
            if (errorCallback) errorCallback(-4, currentCmdId.c_str());
            http.end();
            return false;
        }
    }

    if (written != contentLength) {
        Serial.printf("[OTA] Write incomplete. Written: %d / %d\n", written, contentLength);
        Update.abort();
        if (errorCallback) errorCallback(-5, currentCmdId.c_str());
        http.end();
        return false;
    }

    if (!Update.end()) {
        Serial.printf("[OTA] End Failed. Error: %s\n", Update.errorString());
        if (errorCallback) {
            errorCallback(Update.getError(), currentCmdId.c_str());
        }
        http.end();
        return false;
    }

    if (!Update.isFinished()) {
        Serial.println("[OTA] Update not finished");
        if (errorCallback) errorCallback(-6, currentCmdId.c_str());
        http.end();
        return false;
    }

    Serial.println("[OTA] Update Success! Rebooting in 2 seconds...");
    if (finishCallback) finishCallback(currentCmdId.c_str());
    http.end();
    
    delay(2000);
    ESP.restart();
    
    return true;
}