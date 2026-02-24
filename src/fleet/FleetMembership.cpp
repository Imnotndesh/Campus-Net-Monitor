#include "FleetMembership.h"
#include <ArduinoJson.h>

bool FleetMembership::initialized = false;

void FleetMembership::begin() {
    if (initialized) return;
    initialized = true;
}

std::vector<String> FleetMembership::getGroups() {
    std::vector<String> groups;
    String groupsStr = ConfigManager::getFleetGroups();
    
    if (groupsStr.length() == 0) return groups;
    
    int start = 0;
    int end = groupsStr.indexOf(',');
    while (end > 0) {
        groups.push_back(groupsStr.substring(start, end));
        start = end + 1;
        end = groupsStr.indexOf(',', start);
    }
    if (start < groupsStr.length()) {
        groups.push_back(groupsStr.substring(start));
    }
    
    return groups;
}

bool FleetMembership::isInGroup(String group) {
    std::vector<String> groups = getGroups();
    for (const String& g : groups) {
        if (g.equals(group)) return true;
    }
    return false;
}

void FleetMembership::addGroup(String group) {
    if (isInGroup(group)) return;
    
    std::vector<String> groups = getGroups();
    groups.push_back(group);
    
    String newGroups;
    for (size_t i = 0; i < groups.size(); i++) {
        if (i > 0) newGroups += ",";
        newGroups += groups[i];
    }
    
    ConfigManager::setFleetGroups(newGroups);
}

void FleetMembership::removeGroup(String group) {
    std::vector<String> groups = getGroups();
    std::vector<String> newGroups;
    
    for (const String& g : groups) {
        if (!g.equals(group)) {
            newGroups.push_back(g);
        }
    }
    
    if (newGroups.size() == groups.size()) return;
    
    String newGroupsStr;
    for (size_t i = 0; i < newGroups.size(); i++) {
        if (i > 0) newGroupsStr += ",";
        newGroupsStr += newGroups[i];
    }
    
    ConfigManager::setFleetGroups(newGroupsStr);
}

String FleetMembership::getLocation() {
    return ConfigManager::getFleetLocation();
}

void FleetMembership::setLocation(String location) {
    ConfigManager::setFleetLocation(location);
}

String FleetMembership::getTag(String key) {
    String tagsStr = ConfigManager::getFleetTags();
    if (tagsStr.length() == 0 || tagsStr == "{}") return "";
    
    DynamicJsonDocument doc(512);
    deserializeJson(doc, tagsStr);
    
    return doc[key].as<String>();
}

void FleetMembership::setTag(String key, String value) {
    String tagsStr = ConfigManager::getFleetTags();
    DynamicJsonDocument doc(512);
    
    if (tagsStr.length() > 0 && tagsStr != "{}") {
        deserializeJson(doc, tagsStr);
    }
    
    doc[key] = value;
    
    String newTags;
    serializeJson(doc, newTags);
    ConfigManager::setFleetTags(newTags);
}

void FleetMembership::removeTag(String key) {
    String tagsStr = ConfigManager::getFleetTags();
    if (tagsStr.length() == 0 || tagsStr == "{}") return;
    
    DynamicJsonDocument doc(512);
    deserializeJson(doc, tagsStr);
    
    if (doc.containsKey(key)) {
        doc.remove(key);
        
        String newTags;
        serializeJson(doc, newTags);
        ConfigManager::setFleetTags(newTags);
    }
}

String FleetMembership::getMembershipJson() {
    DynamicJsonDocument doc(512);
    
    doc["groups"] = ConfigManager::getFleetGroups();
    doc["location"] = ConfigManager::getFleetLocation();
    
    String tagsStr = ConfigManager::getFleetTags();
    if (tagsStr.length() > 0 && tagsStr != "{}") {
        DynamicJsonDocument tagsDoc(512);
        deserializeJson(tagsDoc, tagsStr);
        doc["tags"] = tagsDoc;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

String FleetMembership::getMembershipHash() {
    String membership = getMembershipJson();
    
    unsigned long hash = 5381;
    for (size_t i = 0; i < membership.length(); i++) {
        hash = ((hash << 5) + hash) + membership.charAt(i);
    }
    
    char hashStr[17];
    snprintf(hashStr, sizeof(hashStr), "%08lx", hash);
    return String(hashStr);
}