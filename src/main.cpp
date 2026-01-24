#include <Arduino.h>
#include "storage/StorageManager.h"
#include "storage/ConfigManager.h"
#include "connection/ConnectionManager.h"
#include "comms/MqttManager.h"
#include "diagnostics/DiagnosticEngine.h"
#include "packaging/JsonPackager.h"
#include "packaging/TimeManager.h"
#include "firmware/stability/WatchdogManager.h"
#include "firmware/OTAManager.h"
#include "actions/led/StatusLED.h"
#include "actions/button/ButtonManager.h"

const int LED_PIN = 2;
const int BOOT_PIN = 0;

enum SystemState { PORTAL, RUNNING };

const uint32_t WATCHDOG_TIMEOUT = 60;
const char* PROBE_ID = "PROBE-SEC-05";
const char* FIRMWARE_VERSION = "1.0.1";
const char* OTA_UPDATE_URL = "http://192.168.100.27:8080/firmware.bin";
const uint32_t NTP_SYNC_INTERVAL = 3600000;

SystemState currentState = PORTAL;
SystemConfig activeCfg;
unsigned long lastTelemetryReport = 0;
unsigned long lastWatchdogKick = 0;
bool deepScanPending = false;
volatile bool ntpSynced = false;

void ntpSyncTask(void * pvParameters) {
    const TickType_t syncInterval = pdMS_TO_TICKS(NTP_SYNC_INTERVAL);
    const TickType_t retryDelay = pdMS_TO_TICKS(30000);
    const TickType_t initialDelay = pdMS_TO_TICKS(5000);
    
    vTaskDelay(initialDelay);
    
    for(;;) {
        if (currentState == RUNNING && ConnectionManager::isConnected()) {
            if (!ntpSynced || TimeManager::getEpoch() == 0) {
                Serial.print("[TIME] Attempting NTP sync");
                TimeManager::sync();
                
                uint32_t epoch = TimeManager::getEpoch();
                if (epoch > 1700000000) {
                    ntpSynced = true;
                    Serial.printf("[TIME] ✓ Synced: %s (Epoch: %u)\n", 
                                  TimeManager::getTimestamp().c_str(), epoch);
                    vTaskDelay(syncInterval);
                } else {
                    Serial.printf("[TIME] ✗ Invalid epoch: %u (retrying in 30s)\n", epoch);
                    vTaskDelay(retryDelay);
                }
            } else {
                Serial.print("[TIME] Re-sync");
                TimeManager::sync();
                Serial.printf(" ✓ %s\n", TimeManager::getTimestamp().c_str());
                vTaskDelay(syncInterval);
            }
        } else {
            if (ntpSynced) {
                Serial.println("[TIME] WiFi down - marking time as unsynced");
            }
            ntpSynced = false;
            vTaskDelay(retryDelay);
        }
    }
}

void uiTask(void * pvParameters) {
    const TickType_t xDelay = 20 / portTICK_PERIOD_MS;
    for(;;) {
        StatusLED::update();
        ButtonManager::update();
        vTaskDelay(xDelay);
    }
}

bool performSystemBootstrap() {
    Serial.println("\n╔══════════════════════════════════════════════════════════╗");
    Serial.println("║  Campus Network Monitor - Diagnostic Probe              ║");
    Serial.println("║  Firmware Version: 1.0.1                                 ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝\n");

    WifiCredentials creds = StorageManager::loadWifiCredentials();
    bool hasWifi = StorageManager::hasCredentials();
    bool hasMqtt = ConfigManager::isConfigured();
    
    Serial.printf("[BOOT] WiFi Credentials: %s\n", hasWifi ? "✓" : "✗");
    Serial.printf("[BOOT] MQTT Configuration: %s\n", hasMqtt ? "✓" : "✗");
    Serial.printf("[BOOT] Failure Count: %d/%d\n", 
                  StorageManager::getFailureCount(), 
                  ConnectionManager::MAX_FAILURES);

    if (hasWifi && hasMqtt) {
        if (ConnectionManager::establishConnection(creds.ssid, creds.password)) {
            activeCfg = ConfigManager::load();
            MqttManager::setup(activeCfg.mqttServer, activeCfg.mqttPort, PROBE_ID);
            TimeManager::begin();
            Serial.println("[BOOT] ✓ System Ready - Entering RUNNING state");
            return true;
        }
    }
    
    Serial.println("[BOOT] ✗ Bootstrap Failed - Entering PORTAL state");
    return false;
}

void performLightScan() {
    if (!ntpSynced || TimeManager::getEpoch() == 0) {
        Serial.println("[SCAN] ⚠ Skipping - NTP not synced");
        return;
    }
    
    Serial.println("\n[SCAN] ═══════════════════════════════════════════");
    Serial.println("[SCAN] Starting Light Telemetry Scan...");
    
    WatchdogManager::reset();
    NetworkMetrics metrics = DiagnosticEngine::performFullTest("8.8.8.8");
    String payload = JsonPackager::serializeLight(metrics, PROBE_ID);
    
    Serial.printf("[SCAN] Timestamp: %s (Epoch: %u)\n", 
                  TimeManager::getTimestamp().c_str(),
                  TimeManager::getEpoch());
    
    if (MqttManager::publishTelemetry(payload)) {
        Serial.println("[SCAN] ✓ Telemetry Published");
        Serial.printf("[SCAN] RSSI: %d dBm | Latency: %d ms | Loss: %.1f%%\n",
                      metrics.rssi, metrics.avgLatency, metrics.packetLoss);
    } else {
        Serial.println("[SCAN] ⚠ MQTT Unavailable - Buffered to Flash");
    }
    
    Serial.println("[SCAN] ═══════════════════════════════════════════\n");
}

void performDeepScan() {
    if (!ntpSynced || TimeManager::getEpoch() == 0) {
        Serial.println("[DEEP] ⚠ Skipping - NTP not synced");
        MqttManager::clearDeepScanFlag();
        deepScanPending = false;
        return;
    }
    
    Serial.println("\n[DEEP] ═══════════════════════════════════════════");
    Serial.println("[DEEP] Starting Deep Analysis (Radio Sniffer)...");
    Serial.println("[DEEP] ⚠ Temporarily disconnecting from WiFi...");
    
    WatchdogManager::reset();
    EnhancedMetrics enhanced = DiagnosticEngine::performDeepAnalysis("8.8.8.8");
    String payload = JsonPackager::serializeEnhanced(enhanced, PROBE_ID);
    
    WifiCredentials creds = StorageManager::loadWifiCredentials();
    if (ConnectionManager::tryConnect(creds.ssid, creds.password)) {
        Serial.println("[DEEP] ✓ Reconnected to Network");
        ntpSynced = false;
        
        if (MqttManager::publishTelemetry(payload)) {
            Serial.println("[DEEP] ✓ Enhanced Telemetry Published");
            Serial.printf("[DEEP] Timestamp: %s\n", TimeManager::getTimestamp().c_str());
            Serial.printf("[DEEP] Link Quality: %.1f/100 | SNR: %.1f dB | Utilization: %.1f%%\n",
                          enhanced.linkQuality, enhanced.snr, enhanced.channelUtilization);
        }
    } else {
        Serial.println("[DEEP] ✗ Reconnection Failed - Buffering Results");
        StorageManager::appendToBuffer(payload);
    }
    
    Serial.println("[DEEP] ═══════════════════════════════════════════\n");
    MqttManager::clearDeepScanFlag();
    deepScanPending = false;
}

void handleConnectionLoss() {
    Serial.println("[ERROR] WiFi Connection Lost");
    StatusLED::setStatus(ERR_WIFI);
    
    WifiCredentials creds = StorageManager::loadWifiCredentials();
    if (!ConnectionManager::establishConnection(creds.ssid, creds.password)) {
        Serial.println("[ERROR] Recovery Failed - Entering PORTAL mode");
        currentState = PORTAL;
        ConnectionManager::startCaptivePortal();
        ntpSynced = false;
    } else {
        Serial.println("[RECOVERY] ✓ WiFi Reconnected");
        ntpSynced = false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    StatusLED::begin(LED_PIN);
    ButtonManager::begin(BOOT_PIN);
    StatusLED::setStatus(MODE_PORTAL);
    
    StorageManager::begin();
    ConfigManager::begin();
    
    xTaskCreatePinnedToCore(uiTask, "uiTask", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(ntpSyncTask, "ntpSync", 4096, NULL, 1, NULL, 0);
    
    Serial.println("[SYSTEM] UI Task Started on Core 0");
    Serial.println("[SYSTEM] NTP Sync Task Started on Core 0");
    
    WatchdogManager::begin(WATCHDOG_TIMEOUT);
    
    if (performSystemBootstrap()) {
        currentState = RUNNING;
        StatusLED::setStatus(STATUS_OK);
    } else {
        currentState = PORTAL;
        StatusLED::setStatus(MODE_PORTAL);
        ConnectionManager::startCaptivePortal();
    }
    
    WatchdogManager::reset();
    lastWatchdogKick = millis();
}

void loop() {
    if (millis() - lastWatchdogKick >= 30000) {
        WatchdogManager::reset();
        lastWatchdogKick = millis();
    }
    
    if (currentState == PORTAL) {
        StatusLED::setStatus(MODE_PORTAL);
        ConnectionManager::handlePortal();
    } else {
        if (!ConnectionManager::isConnected()) {
            handleConnectionLoss();
            return;
        }
        
        MqttManager::loop();
        
        if (MqttManager::isConnected()) {
            StatusLED::setStatus(STATUS_OK);
        } else {
            StatusLED::setStatus(ERR_MQTT);
        }
        
        if (MqttManager::isDeepScanRequested() && !deepScanPending) {
            Serial.println("[CMD] Deep Scan Requested via MQTT");
            deepScanPending = true;
        }
        
        if (deepScanPending) {
            performDeepScan();
        }
        
        unsigned long currentMillis = millis();
        unsigned long reportInterval = activeCfg.reportInterval * 1000UL;
        
        if (currentMillis - lastTelemetryReport >= reportInterval) {
            lastTelemetryReport = currentMillis;
            performLightScan();
        }
    }
    
    delay(10);
}