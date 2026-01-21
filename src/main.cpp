#include <Arduino.h>
#include "storage/StorageManager.h"
#include "connection/ConnectionManager.h"

enum DeviceState { TRY_CONNECT, CAPTIVE_PORTAL, DIAGNOSTICS, SHUTDOWN };
DeviceState currentState;
unsigned long portalStartTime = 0;
const unsigned long PORTAL_TIMEOUT = 5 * 60 * 1000;

void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); } 
    delay(2000); 

    Serial.println("\n--- Probe Startup ---");
    StorageManager::begin();
    ConnectionManager::begin();

    int count = StorageManager::getRebootCount();
    if (count >= 2) {
        currentState = SHUTDOWN;
    } else {
        currentState = TRY_CONNECT;
    }
}

void loop() {
    switch (currentState) {
        case TRY_CONNECT: {
            WifiCredentials creds = StorageManager::loadWifiCredentials();
            Serial.printf("Searching for stored credentials... (%s)\n", creds.ssid.c_str());

            if (ConnectionManager::tryConnect(creds.ssid, creds.password)) {
                Serial.println("Connection Successful!");
                currentState = DIAGNOSTICS;
                StorageManager::setRebootCount(0); // Success, so reset the fail counter
            } else {
                Serial.println("Connection Failed. Opening Portal...");
                currentState = CAPTIVE_PORTAL;
                portalStartTime = millis();
                ConnectionManager::startCaptivePortal();
            }
            break;
        }

        case DIAGNOSTICS: {
            static unsigned long lastTest = 0;
            if (millis() - lastTest > 5000) {
                Serial.println("[DIAG] Performing Network Tests...");
                // Check if we lost connection
                if (!ConnectionManager::isConnected()) {
                    Serial.println("[DIAG] Connection lost! Reverting to TRY_CONNECT");
                    currentState = TRY_CONNECT;
                }
                lastTest = millis();
            }
            break;
        }

        case CAPTIVE_PORTAL: {
            ConnectionManager::handlePortal();
            // ... (rest of your portal timeout logic)
            break;
        }

        case SHUTDOWN:
            Serial.println("Powering down to preserve state.");
            ESP.deepSleep(0);
            break;
    }
}