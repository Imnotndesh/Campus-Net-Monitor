#include "OTAManager.h"
#include <WiFiClient.h>

void OTAManager::checkForUpdates(const char* updateUrl, const char* currentVersion) {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[OTA] Checking for updates...");
    
    // Create a WiFiClient for the update process
    WiFiClient client;

    // Set callbacks for visual/log feedback
    httpUpdate.onStart(update_started);
    httpUpdate.onEnd(update_finished);
    httpUpdate.onProgress(update_progress);
    httpUpdate.onError(update_error);

    // Provide the 'client' as the first parameter
    t_httpUpdate_return ret = httpUpdate.update(client, updateUrl, currentVersion);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Update Failed (Error %d): %s\n", 
                          httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No updates available.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Update successful. Rebooting...");
            break;
    }
}

// Ensure these functions remain in the file
void OTAManager::update_started() { Serial.println("[OTA] Update process started."); }
void OTAManager::update_finished() { Serial.println("[OTA] Update process finished."); }
void OTAManager::update_progress(int cur, int total) { 
    Serial.printf("[OTA] Progress: %d%%\n", (cur * 100) / total); 
}
void OTAManager::update_error(int err) { Serial.printf("[OTA] Error Code: %d\n", err); }