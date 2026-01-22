#include <Arduino.h>
#include "storage/StorageManager.h"
#include "storage/ConfigManager.h"
#include "connection/ConnectionManager.h"
#include "comms/MqttManager.h"
#include "diagnostics/DiagnosticEngine.h"
#include "packaging/JsonPackager.h"
#include "actions/led/StatusLED.h"
#include "actions/button/ButtonManager.h"

// System States
enum SystemState { PORTAL, RUNNING };
SystemState currentState;
SystemConfig activeCfg;

// Pin Definitions (Standard DevKit V1)
const int LED_PIN = 2;
const int BOOT_PIN = 0;
void uiTask(void * pvParameters) {
    for(;;) {
        StatusLED::update();
        ButtonManager::update();
        vTaskDelay(20 / portTICK_PERIOD_MS); 
    }
}
void setup() {
    Serial.begin(115200);
    delay(1000);
    StatusLED::begin(LED_PIN);
    ButtonManager::begin(BOOT_PIN);
    StatusLED::setStatus(MODE_PORTAL);

    // 2. Initialize Storage
    StorageManager::begin();
    ConfigManager::begin();

    xTaskCreatePinnedToCore(uiTask, "uiTask", 2048, NULL, 1, NULL, 0);
    // 3. Load Credentials
    WifiCredentials creds = StorageManager::loadWifiCredentials();
    
    Serial.println("\n--- [Campus Monitor probe startup] ---");
    
    // 4. Unified Connection Logic
    // This handles: Connection -> Retry Count -> Portal Trigger
    if (ConnectionManager::establishConnection(creds.ssid, creds.password)) {
        activeCfg = ConfigManager::load();
        
        // Setup MQTT with current config
        MqttManager::setup(activeCfg.mqttServer, activeCfg.mqttPort, "PROBE-SEC-05");
        
        Serial.println("[SYSTEM] Transitioning to RUNNING state.");
        currentState = RUNNING;
    } else {
        Serial.println("[SYSTEM] Transitioning to PORTAL state.");
        currentState = PORTAL;
    }
}

void loop() {
    if (currentState == PORTAL) {
        StatusLED::setStatus(MODE_PORTAL); // 1 Slow Pulse
        ConnectionManager::handlePortal();
    } 
    else {
        // PRODUCTION MODE
        if (ConnectionManager::isConnected()) {
            MqttManager::loop();

            // Update LED based on Comms Health
            if (MqttManager::isConnected()) {
                StatusLED::setStatus(STATUS_OK); // Solid Blue
            } else {
                StatusLED::setStatus(ERR_MQTT); // 5 Pulses (Bad IP/Port)
            }

            // Standard Telemetry Cycle
            static unsigned long lastReport = 0;
            if (millis() - lastReport > (activeCfg.reportInterval * 1000)) {
                lastReport = millis();
                
                Serial.println("[DIAG] Running Telemetry Scan...");
                NetworkMetrics m = DiagnosticEngine::performFullTest("8.8.8.8");
                String payload = JsonPackager::serializeLight(m, "PROBE-SEC-05");
                
                if (MqttManager::publishTelemetry(payload)) {
                    Serial.println("[MQTT] Published.");
                } else {
                    Serial.println("[MQTT] Buffer Saved.");
                }
            }
        } 
        else {
            // WiFi Dropped
            StatusLED::setStatus(ERR_WIFI); // 4 Pulses
            
            // Try to re-establish or fall back to portal
            WifiCredentials creds = StorageManager::loadWifiCredentials();
            if (!ConnectionManager::establishConnection(creds.ssid, creds.password)) {
                currentState = PORTAL;
            }
        }
    }
}