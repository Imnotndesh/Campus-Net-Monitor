#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "../storage/StorageManager.h"

class ConnectionManager {
public:
    static void begin();
    static bool tryConnect(String ssid, String pass);
    static void startCaptivePortal();
    static void handlePortal(); // Must be called in loop() during portal mode
    static bool isConnected();

private:
    static void handleRoot();
    static void handleSave();
    static WebServer server;
    static DNSServer dnsServer;
};

#endif