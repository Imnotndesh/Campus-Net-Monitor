// src/main.cpp (Updated)
#include <Arduino.h>
#include "storage/StorageManager.h"
#include "storage/ConfigManager.h"
#include "connection/ConnectionManager.h"
#include "comms/MqttManager.h"
#include "comms/CommandHandler.h"
#include "diagnostics/DiagnosticEngine.h"
#include "packaging/JsonPackager.h"
#include "packaging/TimeManager.h"
#include "actions/led/StatusLED.h"
#include "actions/button/ButtonManager.h"

// System States
enum SystemState { PORTAL, RUNNING };
SystemState currentState;
SystemConfig activeCfg;

// Pin Definitions
const int LED_PIN = 2;
const int BOOT_PIN = 0;

// Probe ID - Should be unique per device
const String PROBE_ID = "PROBE-SEC-05";

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
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   Campus Monitor Probe v1.0.0          ║");
    Serial.println("║   Network Diagnostic System            ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    StatusLED::begin(LED_PIN);
    ButtonManager::begin(BOOT_PIN);
    StatusLED::setStatus(MODE_PORTAL);

    // Initialize Storage & Config
    StorageManager::begin();
    ConfigManager::begin();
    CommandHandler::begin();

    // Start UI task
    xTaskCreatePinnedToCore(uiTask, "uiTask", 2048, NULL, 1, NULL, 0);
    
    // Load WiFi Credentials
    WifiCredentials creds = StorageManager::loadWifiCredentials();
    
    Serial.println("[SYSTEM] Starting connection sequence...");
    
    // Establish Connection
    if (ConnectionManager::establishConnection(creds.ssid, creds.password)) {
        activeCfg = ConfigManager::load();
        
        // Initialize Time
        TimeManager::begin();
        TimeManager::sync();
        
        // Setup MQTT
        MqttManager::setup(activeCfg.mqttServer, activeCfg.mqttPort, PROBE_ID);
        
        Serial.println("[SYSTEM] ✓ Transitioning to RUNNING state");
        Serial.printf("[SYSTEM] ✓ Probe ID: %s\n", PROBE_ID.c_str());
        Serial.printf("[SYSTEM] ✓ Report Interval: %ds\n", activeCfg.reportInterval);
        Serial.printf("[SYSTEM] ✓ MQTT: %s:%d\n", activeCfg.mqttServer, activeCfg.mqttPort);
        currentState = RUNNING;
    } else {
        Serial.println("[SYSTEM] ✗ Transitioning to PORTAL state");
        currentState = PORTAL;
    }
}

void loop() {
    if (currentState == PORTAL) {
        StatusLED::setStatus(MODE_PORTAL);
        ConnectionManager::handlePortal();
    } 
    else {
        if (ConnectionManager::isConnected()) {
            MqttManager::loop();
            if (MqttManager::isConnected()) {
                StatusLED::setStatus(STATUS_OK);
            } else {
                StatusLED::setStatus(ERR_MQTT);
            }
            static unsigned long lastReport = 0;
            if (millis() - lastReport > (activeCfg.reportInterval * 1000)) {
                lastReport = millis();
                
                Serial.println("\n[DIAG] ═══ Telemetry Cycle ═══");
                NetworkMetrics m = DiagnosticEngine::performFullTest("8.8.8.8");
                String payload = JsonPackager::serializeLight(m, PROBE_ID);
                
                if (MqttManager::publishTelemetry(payload)) {
                    Serial.println("[MQTT] ✓ Telemetry published");
                } else {
                    Serial.println("[MQTT] ✗ Telemetry buffered offline");
                }
            }
            if (MqttManager::isDeepScanRequested()) {
                Serial.println("\n[DIAG] ═══ Deep Scan Requested ═══");
                EnhancedMetrics em = DiagnosticEngine::performDeepAnalysis("8.8.8.8");
                String payload = JsonPackager::serializeEnhanced(em, PROBE_ID);
                
                if (MqttManager::publishTelemetry(payload)) {
                    Serial.println("[MQTT] ✓ Deep scan published");
                } else {
                    Serial.println("[MQTT] ✗ Deep scan buffered offline");
                }
                
                MqttManager::clearDeepScanFlag();
            }
        } 
        else {
            StatusLED::setStatus(ERR_WIFI);
            
            WifiCredentials creds = StorageManager::loadWifiCredentials();
            if (!ConnectionManager::establishConnection(creds.ssid, creds.password)) {
                currentState = PORTAL;
            }
        }
    }
}