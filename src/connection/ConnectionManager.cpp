#include "ConnectionManager.h"

WebServer ConnectionManager::server(80);
DNSServer ConnectionManager::dnsServer;
const byte ConnectionManager::DNS_PORT = 53;
bool ConnectionManager::isPortalActive = false;

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

void ConnectionManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
}

bool ConnectionManager::tryConnect(String ssid, String pass) {
    if (ssid == "") return false;

    Serial.printf("[CONN] Connecting to %s...", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[CONN] Connected! IP: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\n[CONN] Connection Failed.");
        return false;
    }
}

void ConnectionManager::startCaptivePortal() {
    isPortalActive = true;
    WiFi.mode(WIFI_AP);

    WiFi.softAPConfig(apIP, apIP, netMsk);

    uint32_t chipId = (uint32_t)ESP.getEfuseMac();
    String apName = "Probe-Setup-" + String(chipId, HEX);
    WiFi.softAP(apName.c_str());

    Serial.println("[PORTAL] AP Started: " + apName);
    Serial.print("[PORTAL] IP Address: ");
    Serial.println(WiFi.softAPIP());

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    if (MDNS.begin("setup")) {
        Serial.println("[PORTAL] Setup UI started at: http://setup.local");
    } else {
        Serial.println("[PORTAL] Error setting up mDNS responder!");
    }

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/generate_204", handleRoot);
    server.on("/gen_204", handleRoot);
    server.on("/hotspot-detect.html", handleRoot); 
    server.on("/fwlink", handleRoot);
    
    server.onNotFound(handleNotFound);
    server.begin();
}

void ConnectionManager::handlePortal() {
    if (isPortalActive) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

void ConnectionManager::handleRoot() {
    SystemConfig currentCfg = ConfigManager::load();
    String currentProbeId = String(currentCfg.probe_id);
    if (currentProbeId.length() == 0) currentProbeId = "PROBE-NEW";

    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Campus Probe Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #f2f2f7; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
    .card { background: white; padding: 2rem; border-radius: 12px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); width: 100%; max-width: 400px; }
    h1 { color: #1c1c1e; margin-bottom: 1.5rem; text-align: center; font-size: 24px; }
    h3 { color: #8e8e93; font-size: 14px; text-transform: uppercase; margin-top: 1.5rem; margin-bottom: 0.5rem; letter-spacing: 0.5px; }
    input { width: 100%; padding: 12px; margin-bottom: 10px; border: 1px solid #d1d1d6; border-radius: 8px; box-sizing: border-box; font-size: 16px; }
    input:focus { border-color: #007aff; outline: none; }
    input[type="submit"] { background-color: #007aff; color: white; font-weight: bold; border: none; cursor: pointer; margin-top: 1rem; transition: background 0.2s; }
    input[type="submit"]:hover { background-color: #0056b3; }
    .note { font-size: 12px; color: #8e8e93; text-align: center; margin-top: 1rem; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Probe Setup</h1>
    <form action="/save" method="POST">
      
      <h3>WiFi Network</h3>
      <input type="text" name="ssid" placeholder="SSID (Network Name)" required>
      <input type="password" name="pass" placeholder="Password">
      
      <h3>Configuration</h3>
      <input type="text" name="probe_id" placeholder="Probe ID" value=")rawliteral" + currentProbeId + R"rawliteral(">
      <input type="text" name="mqtt_srv" placeholder="MQTT Broker IP" value=")rawliteral" + String(currentCfg.mqttServer) + R"rawliteral(">
      <input type="number" name="mqtt_port" placeholder="MQTT Port" value=")rawliteral" + String(currentCfg.mqttPort) + R"rawliteral(">
      
      <input type="submit" value="Save & Connect">
    </form>
    <div class="note">Device will reboot after saving.</div>
  </div>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}

void ConnectionManager::handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    String probeId = server.arg("probe_id");
    String mqttSrv = server.arg("mqtt_srv");
    int mqttPort = server.arg("mqtt_port").toInt();

    if (ssid.length() > 0 && mqttSrv.length() > 0) {
        StorageManager::saveWifiCredentials(ssid, pass);
        SystemConfig cfg = ConfigManager::load();
        strncpy(cfg.mqttServer, mqttSrv.c_str(), sizeof(cfg.mqttServer));
        strncpy(cfg.probe_id, probeId.c_str(), sizeof(cfg.probe_id));
        cfg.mqttPort = mqttPort;
        
        // Ensure defaults if missing
        if (strlen(cfg.telemetryTopic) == 0) strncpy(cfg.telemetryTopic, "campus/probes/telemetry", sizeof(cfg.telemetryTopic));
        
        ConfigManager::save(cfg);
        StorageManager::resetFailureCount();
        StorageManager::setRebootCount(0);
        
        Serial.println("[PORTAL] Config saved. Rebooting...");
        
        String html = "<html><head><meta name='viewport' content='width=device-width'><style>body{font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;text-align:center;}</style></head><body><div><h1>Saved!</h1><p>Probe is restarting...</p><p>Connect your device back to the main WiFi.</p></div></body></html>";
        server.send(200, "text/html", html);
        
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/html", "Error: Missing required fields.");
    }
}

void ConnectionManager::handleNotFound() {
    if (isPortalActive) {
        server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "Not Found");
    }
}

String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

bool ConnectionManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool ConnectionManager::establishConnection(String ssid, String pass) {
    int fails = StorageManager::getFailureCount();
    if (fails >= MAX_FAILURES) {
        Serial.println("[CONN] Too many failures. Starting Portal.");
        startCaptivePortal();
        return false;
    }

    if (tryConnect(ssid, pass)) {
        StorageManager::resetFailureCount();
        return true;
    } else {
        StorageManager::incrementFailureCount();
        Serial.printf("[CONN] Failed. Attempt %d/%d\n", StorageManager::getFailureCount(), MAX_FAILURES);
        if (StorageManager::getFailureCount() >= MAX_FAILURES) {
            startCaptivePortal();
            return false;
        }
        delay(1000);
        ESP.restart();
        return false; 
    }
}