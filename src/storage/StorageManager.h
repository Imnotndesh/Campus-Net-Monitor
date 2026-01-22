#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>

struct WifiCredentials {
    String ssid;
    String password;
};

class StorageManager {
public:
    static void begin();
    
    // NVS Methods (for Credentials & State)
    static void saveWifiCredentials(String ssid, String pass);
    static WifiCredentials loadWifiCredentials();
    static void setRebootCount(int count);
    static int getRebootCount();
    static int getFailureCount();
    static void incrementFailureCount();
    static void resetFailureCount();

    // LittleFS Methods (for Data Buffering)
    static bool appendToBuffer(String jsonPayload);
    static String readBuffer();
    static void clearBuffer();
    static size_t getBufferSize();
    static bool hasCredentials();
    static void wipe();
};

#endif