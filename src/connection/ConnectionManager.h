#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "../storage/StorageManager.h"
#include "../storage/ConfigManager.h"

#define MAX_FAILURES 3

class ConnectionManager {
public:
    static void begin();
    static bool establishConnection(String ssid, String pass);
    static void handlePortal(); // Call this in loop()
    static bool isConnected();

private:
    static WebServer server;
    static DNSServer dnsServer;
    static const byte DNS_PORT;
    static bool isPortalActive;

    static bool tryConnect(String ssid, String pass);
    static void startCaptivePortal();
    static void handleRoot();
    static void handleSave();
    static void handleNotFound();
};

#endif