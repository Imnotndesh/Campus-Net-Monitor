#ifndef RESULT_BUFFER_H
#define RESULT_BUFFER_H

#include <Arduino.h>
#include <LittleFS.h> // Fixed: Changed from SPIFFS to LittleFS
#include <ArduinoJson.h>

#define MAX_BUFFERED_RESULTS 10
#define RESULT_BUFFER_FILE "/result_buffer.json"

struct BufferedResult {
    String cmdType;
    String status;
    String resultJson;
    unsigned long timestamp;
};

class ResultBuffer {
public:
    static void begin();
    static bool saveResult(String cmdType, String status, String resultJson);
    static bool hasBufferedResults();
    static BufferedResult getNextResult();
    static void clearResult();
    static int getBufferCount();
    static void clearAll();

private:
    static bool loadBuffer();
    static bool saveBuffer();
    static DynamicJsonDocument buffer;
    static bool initialized;
};

#endif