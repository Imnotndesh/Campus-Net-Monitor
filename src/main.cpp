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

// --- Global State ---
enum SystemState { PORTAL, RUNNING };
SystemState currentState;
SystemConfig activeCfg;

const int LED_PIN = 2;
const int BOOT_PIN = 0;

void setupSystem();
void startPortalMode();
void startRunningMode();
void handleRunningState();
void performTelemetry();

void uiTask(void * pvParameters) {
    for(;;) {
        StatusLED::update();
        ButtonManager::update();
        vTaskDelay(20 / portTICK_PERIOD_MS); 
    }
}

void setup() {
    setupSystem();
    WifiCredentials creds = StorageManager::loadWifiCredentials();
    activeCfg = ConfigManager::load();

    Serial.println("[SYSTEM] Starting connection sequence...");
    if (ConnectionManager::establishConnection(creds.ssid, creds.password)) {
        startRunningMode();
    } else {
        startPortalMode();
    }
}

void loop() {
    if (currentState == PORTAL) {
        ConnectionManager::handlePortal();
    } 
    else {
        handleRunningState();
    }
}

void setupSystem() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║   Campus Monitor Probe v1.0.0          ║");
    Serial.println("║   Network Diagnostic System            ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    // Hardware Init
    StatusLED::begin(LED_PIN);
    ButtonManager::begin(BOOT_PIN);
    
    // Subsystem Init
    StorageManager::begin();
    ConfigManager::begin();
    CommandHandler::begin();
    
    // Start UI Task
    xTaskCreatePinnedToCore(uiTask, "uiTask", 2048, NULL, 1, NULL, 0);
}

void startPortalMode() {
    Serial.println("[SYSTEM] ✗ Transitioning to PORTAL state");
    StatusLED::setStatus(MODE_PORTAL);
    currentState = PORTAL;
}

void startRunningMode() {
    TimeManager::begin();
    TimeManager::sync();

    activeCfg = ConfigManager::load();
    
    if (activeCfg.reportInterval > 1000) {
        activeCfg.reportInterval = 60;
        ConfigManager::save(activeCfg);
    }
    MqttManager::setup(activeCfg.mqttServer, activeCfg.mqttPort, activeCfg.probe_id);
    StatusLED::setStatus(STATUS_OK);
    currentState = RUNNING;
}

void handleRunningState() {
    if (!ConnectionManager::isConnected()) {
        StatusLED::setStatus(ERR_WIFI);
        WifiCredentials creds = StorageManager::loadWifiCredentials();
        if (!ConnectionManager::establishConnection(creds.ssid, creds.password)) {
            startPortalMode();
        }
        return;
    }
    MqttManager::loop();
    
    if (MqttManager::isConnected()) {
        StatusLED::setStatus(STATUS_OK);
    } else {
        StatusLED::setStatus(ERR_MQTT);
    }

    if (MqttManager::hasPendingCommand()) {
    Serial.println("[DEBUG] *** PENDING COMMAND DETECTED ***");
    PendingCommand cmd = MqttManager::getNextCommand();
    Serial.printf("[DEBUG] Processing command: type=%s, id=%s\n", cmd.type.c_str(), cmd.id.c_str());
    CommandHandler::process(cmd);
    MqttManager::clearCommand();
    Serial.println("[DEBUG] *** COMMAND PROCESSING COMPLETE ***");
    }
    
    static unsigned long lastReport = millis();
    unsigned long elapsed = millis() - lastReport;
    unsigned long interval = activeCfg.reportInterval * 1000;
    
    if (elapsed > interval) {
        lastReport = millis();
        performTelemetry();
    }
    
    delay(100);
}

void performTelemetry() {
    Serial.println("\n[DIAG] ═══ Telemetry Cycle ═══");
    NetworkMetrics m = DiagnosticEngine::performFullTest("8.8.8.8");
    
    String payload = JsonPackager::serializeLight(m, activeCfg.probe_id);
    
    if (MqttManager::publishTelemetry(payload)) {
        Serial.println("[MQTT] ✓ Telemetry published");
    } else {
        Serial.println("[MQTT] ✗ Telemetry buffered offline");
    }
}