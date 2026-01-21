#include "StorageManager.h"

Preferences prefs;

bool StorageManager::begin() {
    bool fsOk = LittleFS.begin(true);
    if (!fsOk) Serial.println("LittleFS Mount Failed");
    return fsOk;
}

void StorageManager::saveWifiCredentials(String ssid, String pass) {
    prefs.begin("wifi-creds", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
}

WifiCredentials StorageManager::loadWifiCredentials() {
    prefs.begin("wifi-creds", true);
    WifiCredentials creds;
    creds.ssid = prefs.getString("ssid", "");
    creds.password = prefs.getString("pass", "");
    prefs.end();
    return creds;
}

void StorageManager::setRebootCount(int count) {
    prefs.begin("system-state", false);
    prefs.putInt("reboot_count", count);
    prefs.end();
}

int StorageManager::getRebootCount() {
    prefs.begin("system-state", true);
    int count = prefs.getInt("reboot_count", 0);
    prefs.end();
    return count;
}

// --- LittleFS Operations ---

bool StorageManager::appendToBuffer(String jsonPayload) {
    File file = LittleFS.open("/buffer.json", "a");
    if (!file) return false;
    
    // Add a newline between JSON objects for easier parsing later
    if (file.println(jsonPayload)) {
        file.close();
        return true;
    }
    file.close();
    return false;
}

size_t StorageManager::getBufferSize() {
    File file = LittleFS.open("/buffer.json", "r");
    if (!file) return 0;
    size_t size = file.size();
    file.close();
    return size;
}

void StorageManager::clearBuffer() {
    LittleFS.remove("/buffer.json");
}