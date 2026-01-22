#include "StorageManager.h"

static Preferences wifiPrefs;

void StorageManager::begin() {
    wifiPrefs.begin("wifi-creds", false);
    wifiPrefs.end();
    if (!LittleFS.begin(true)) {
        Serial.println("[STORAGE] LittleFS Mount Failed");
    }
}

void StorageManager::saveWifiCredentials(String ssid, String pass) {
    wifiPrefs.begin("wifi-creds", false);
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("pass", pass);
    wifiPrefs.end(); // Explicitly commit
}

bool StorageManager::hasCredentials() {
    wifiPrefs.begin("wifi-creds", true);
    bool hasSsid = wifiPrefs.isKey("ssid");
    String s = wifiPrefs.getString("ssid", "");
    wifiPrefs.end();
    return (hasSsid && s.length() > 0);
}
void StorageManager::wipe() {
    // 1. Clear WiFi Credentials & Failure Counts
    Preferences wifiPrefs;
    wifiPrefs.begin("wifi-creds", false);
    wifiPrefs.clear();
    wifiPrefs.end();
    Preferences configPrefs;
    configPrefs.begin("sys-cfg", false);
    configPrefs.clear(); 
    configPrefs.end();

    Serial.println("[STORAGE] NVS wiped including failure thresholds.");
}

WifiCredentials StorageManager::loadWifiCredentials() {
    WifiCredentials creds;
    wifiPrefs.begin("wifi-creds", true);
    creds.ssid = wifiPrefs.getString("ssid", "");
    creds.password = wifiPrefs.getString("pass", "");
    wifiPrefs.end();
    return creds;
}

void StorageManager::setRebootCount(int count) {
    wifiPrefs.begin("system-state", false);
    wifiPrefs.putInt("reboot_count", count);
    wifiPrefs.end();
}

int StorageManager::getRebootCount() {
    wifiPrefs.begin("system-state", true);
    int count = wifiPrefs.getInt("reboot_count", 0);
    wifiPrefs.end();
    return count;
}

// --- LittleFS Operations ---
void StorageManager::incrementFailureCount() {
    wifiPrefs.begin("wifi-creds", false);
    int count = wifiPrefs.getInt("fail_count", 0);
    wifiPrefs.putInt("fail_count", count + 1);
    wifiPrefs.end();
}

void StorageManager::resetFailureCount() {
    wifiPrefs.begin("wifi-creds", false);
    wifiPrefs.putInt("fail_count", 0);
    wifiPrefs.end();
}

int StorageManager::getFailureCount() {
    wifiPrefs.begin("wifi-creds", true);
    int count = wifiPrefs.getInt("fail_count", 0);
    wifiPrefs.end();
    return count;
}
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

String StorageManager::readBuffer() {
    if (!LittleFS.exists("/offline_logs.json")) {
        return "";
    }

    File file = LittleFS.open("/offline_logs.json", FILE_READ);
    if (!file) {
        Serial.println("[STORAGE] Failed to open logs for reading");
        return "";
    }

    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    return content;
}