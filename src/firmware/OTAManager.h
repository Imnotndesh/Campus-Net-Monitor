// OTAManager.h
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>

class OTAManager {
public:
    static bool performUpdate(const char* url, const char* cmdId);
    static void setProgressCallback(void (*callback)(int, int, const char*));
    static void setErrorCallback(void (*callback)(int, const char*));
    static void setStartCallback(void (*callback)(const char*));
    static void setFinishCallback(void (*callback)(const char*));

private:
    static void (*progressCallback)(int current, int total, const char* cmdId);
    static void (*errorCallback)(int error, const char* cmdId);
    static void (*startCallback)(const char* cmdId);
    static void (*finishCallback)(const char* cmdId);
    static String currentCmdId;
};

#endif