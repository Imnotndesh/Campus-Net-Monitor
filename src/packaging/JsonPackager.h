#ifndef JSON_PACKAGER_H
#define JSON_PACKAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../diagnostics/DiagnosticEngine.h"

class JsonPackager {
public:
    static String serializeLight(const NetworkMetrics& m, String probeId);
    static String serializeEnhanced(const EnhancedMetrics& em, String probeId);
};

#endif