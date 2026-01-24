#include "ConnectionManager.h"

WebServer ConnectionManager::server(80);
DNSServer ConnectionManager::dnsServer;
const byte DNS_PORT = 53;

void ConnectionManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

bool ConnectionManager::tryConnect(String ssid, String pass) {
    if (ssid == "") return false;

    Serial.printf("Connecting to %s...", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());

    // Wait up to 15 seconds for connection
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\nConnection Failed.");
        return false;
    }
}

void ConnectionManager::startCaptivePortal() {
    WiFi.mode(WIFI_AP);
    // Unique SSID based on ESP ChipID so multiple probes don't clash
    String apName = "Probe-Setup-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    WiFi.softAP(apName.c_str());

    // Redirect all DNS requests to the ESP's IP
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Web Routes
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot); // Redirect 404s to root for captive portal
    server.begin();

    Serial.println("Captive Portal Started: " + apName);
    Serial.print("Portal IP: ");
    Serial.println(WiFi.softAPIP());
}

void ConnectionManager::handlePortal() {
    dnsServer.processNextRequest();
    server.handleClient();
}

void ConnectionManager::handleRoot() {
    String html = "<html><head><title>Probe Setup</title>";
    html += "<style>body{font-family:sans-serif; padding:20px;} input{margin-bottom:10px; width:100%;}</style></head>";
    html += "<body><h1>Campus Probe Setup</h1>";
    html += "<form action='/save' method='POST'>";
    
    html += "<h3>Network Settings</h3>";
    html += "SSID: <input type='text' name='ssid' placeholder='WiFi Name'><br>";
    html += "Password: <input type='password' name='pass' placeholder='WiFi Password'><br>";
    
    html += "<h3>Reporting Settings</h3>";
    html += "MQTT Server IP: <input type='text' name='mqtt_srv' value='192.168.100.27'><br>";
    html += "MQTT Port: <input type='number' name='mqtt_port' value='1883'><br>";
    html += "Probe ID: <input type='text' name='probe_id' value='SEC-05-ECA'><br>";
    
    html += "<input type='submit' value='Save & Commission Probe' style='background:#007bff; color:white; padding:10px; border:none; cursor:pointer;'>";
    html += "</form></body></html>";
    
    server.send(200, "text/html", html);
}

void ConnectionManager::handleSave() {
    // The names here must match the <input name='...'> in handleRoot
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String mqttSrv = server.arg("mqtt_srv");
    int mqttPort = server.arg("mqtt_port").toInt();

    if (ssid.length() > 0 && mqttSrv.length() > 0) {
        // 1. Save WiFi
        StorageManager::saveWifiCredentials(ssid, pass);
        
        // 2. Save MQTT
        SystemConfig cfg;
        strncpy(cfg.mqttServer, mqttSrv.c_str(), 64);
        cfg.mqttPort = mqttPort;
        strncpy(cfg.telemetryTopic, "campus/probes/telemetry", 128);
        strncpy(cfg.cmdTopic, "campus/probes/cmd", 128);
        cfg.reportInterval = 30;
        
        ConfigManager::save(cfg);
        
        // 3. CRITICAL FIX: Reset failure counters when new credentials are saved
        StorageManager::resetFailureCount();
        StorageManager::setRebootCount(0);
        
        Serial.println("[PORTAL] Credentials saved. Resetting failure counters.");
        
        server.send(200, "text/html", "<h1>Success</h1><p>Probe configured. Rebooting...</p>");
        delay(2000);
        ESP.restart();
    } else {
        server.send(400, "text/html", "Error: SSID and MQTT IP are required.");
    }
}

bool ConnectionManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool ConnectionManager::establishConnection(String ssid, String pass) {
    int fails = StorageManager::getFailureCount();

    if (fails >= MAX_FAILURES) {
        Serial.println("[CONN] Critical Failure Threshold reached. Forcing Portal.");
        startCaptivePortal();
        return false; // Tells main we are now in Portal mode
    }

    if (tryConnect(ssid, pass)) {
        StorageManager::resetFailureCount();
        return true; // Connection successful
    } else {
        StorageManager::incrementFailureCount();
        Serial.printf("[CONN] Connection failed. Attempt %d/%d\n", StorageManager::getFailureCount(), MAX_FAILURES);
        
        if (StorageManager::getFailureCount() >= MAX_FAILURES) {
            startCaptivePortal();
            return false;
        }
        
        ESP.restart(); // Reboot to try again fresh
        return false; 
    }
}