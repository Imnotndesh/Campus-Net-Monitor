#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "../storage/StorageManager.h"
#include "../storage/ConfigManager.h" // Add this

class ConnectionManager {
public:
    static void begin();
    static bool tryConnect(String ssid, String pass);
    static void startCaptivePortal();
    static void handlePortal(); 
    static bool isConnected();
    static bool establishConnection(String ssid, String pass);
    static const int MAX_FAILURES = 5;

private:
    static void handleRoot();
    static void handleSave();
    static WebServer server;
    static DNSServer dnsServer;
};

#endif