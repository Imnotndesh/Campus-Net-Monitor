#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

class OTAManager {
public:
    static void checkForUpdates(const char* updateUrl, const char* currentVersion);
private:
    static void update_started();
    static void update_finished();
    static void update_progress(int cur, int total);
    static void update_error(int err);
};

#endif