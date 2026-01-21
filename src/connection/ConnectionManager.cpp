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
    String html = "<h1>Campus Probe Setup</h1>";
    html += "<form action='/save' method='POST'>";
    html += "SSID: <input type='text' name='ssid'><br>";
    html += "Password: <input type='password' name='pass'><br>";
    html += "<input type='submit' value='Save & Reboot'>";
    html += "</form>";
    server.send(200, "text/html", html);
}

void ConnectionManager::handleSave() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");

    if (ssid != "") {
        StorageManager::saveWifiCredentials(ssid, pass);
        StorageManager::setRebootCount(0); // Reset count as we have new data
        server.send(200, "text/html", "Credentials Saved. Rebooting...");
        delay(2000);
        ESP.restart();
    }
}

bool ConnectionManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}