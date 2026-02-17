// OTAManager.cpp
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
    
    if (WiFi.status() != WL_CONNECTED) {
        if (errorCallback) errorCallback(-1, currentCmdId.c_str());
        return false;
    }

    if (startCallback) startCallback(currentCmdId.c_str());
    
    HTTPClient http;
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] HTTP Failed: %d\n", httpCode);
        if (errorCallback) errorCallback(httpCode, currentCmdId.c_str());
        http.end();
        return false;
    }

    int len = http.getSize();
    bool canBegin = Update.begin(len);
    if (!canBegin) {
        Serial.println("[OTA] Not enough space");
        if (errorCallback) errorCallback(-2, currentCmdId.c_str());
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buff[128];
    int lastProgress = -1;

    while (http.connected() && (len > 0 || len == -1)) {
        size_t size = stream->available();
        if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            Update.write(buff, c);
            written += c;

            if (len > 0) {
                len -= c;
                int progress = (written * 100) / (written + len);
                if (progress != lastProgress && progressCallback) {
                    progressCallback(written, written + len, currentCmdId.c_str());
                    lastProgress = progress;
                }
            }
        }
        delay(1);
    }

    if (written != http.getSize()) {
        Serial.printf("[OTA] Write Failed. Written: %d / %d\n", written, http.getSize());
        if (errorCallback) errorCallback(-3, currentCmdId.c_str());
        http.end();
        return false;
    }

    if (!Update.end()) {
        Serial.println("[OTA] End Failed");
        if (errorCallback) errorCallback(-4, currentCmdId.c_str());
        http.end();
        return false;
    }

    if (!Update.isFinished()) {
        Serial.println("[OTA] Update not finished");
        if (errorCallback) errorCallback(-5, currentCmdId.c_str());
        http.end();
        return false;
    }

    Serial.println("[OTA] Update Success!");
    if (finishCallback) finishCallback(currentCmdId.c_str());
    http.end();
    return true;
}