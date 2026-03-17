#include "ButtonManager.h"
#include "../../storage/StorageManager.h"
#include "../../fleet/FleetManager.h"

int ButtonManager::_pin = 0; // BOOT button is GPIO 0
int ButtonManager::_clickCount = 0;
unsigned long ButtonManager::_lastClickTime = 0;

void ButtonManager::begin(int pin) {
    _pin = pin;
    pinMode(_pin, INPUT_PULLUP); 
}

void ButtonManager::update() {
    static bool lastState = HIGH;
    static unsigned long lastDebounceTime = 0;
    bool reading = digitalRead(_pin);

    if (reading != lastState) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > 50) {
        static bool confirmedState = HIGH;
        if (reading != confirmedState) {
            confirmedState = reading;
            if (confirmedState == LOW) {
                unsigned long now = millis();
                
                if (now - _lastClickTime > 3000) {
                    _clickCount = 0;
                }

                _clickCount++;
                _lastClickTime = now;
                Serial.printf("[BUTTON] Click %d/3\n", _clickCount);

                if (_clickCount >= 3) {
                    Serial.println("[SYSTEM] Reset triggered! Wiping NVS...");
                    FleetScheduler::setFactoryResetPending(true);
                    StorageManager::wipe();
                    delay(1000);
                    ESP.restart();
                }
            }
        }
    }
    lastState = reading;
}