#ifndef FLEET_MEMBERSHIP_H
#define FLEET_MEMBERSHIP_H

#include <Arduino.h>
#include <vector>
#include "../storage/ConfigManager.h"
#include "../packaging/JsonPackager.h"

class FleetMembership {
public:
    static void begin();
    
    static std::vector<String> getGroups();
    static bool isInGroup(String group);
    static void addGroup(String group);
    static void removeGroup(String group);
    
    static String getLocation();
    static void setLocation(String location);
    
    static String getTag(String key);
    static void setTag(String key, String value);
    static void removeTag(String key);
    
    static String getMembershipJson();
    static String getMembershipHash();

private:
    static bool initialized;
};

#endif