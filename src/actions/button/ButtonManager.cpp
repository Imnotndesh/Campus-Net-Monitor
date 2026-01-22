#include "ButtonManager.h"
#include "../../storage/StorageManager.h"

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

    // If the switch changed, due to noise or pressing
    if (reading != lastState) {
        lastDebounceTime = millis();
    }

    // Only process the press if it has been stable for 50ms
    if ((millis() - lastDebounceTime) > 50) {
        static bool confirmedState = HIGH;
        if (reading != confirmedState) {
            confirmedState = reading;

            // Detecting "Falling Edge" (Button pushed down)
            if (confirmedState == LOW) {
                unsigned long now = millis();
                
                // Reset count if the window (3 seconds) has expired
                if (now - _lastClickTime > 3000) {
                    _clickCount = 0;
                }

                _clickCount++;
                _lastClickTime = now;
                Serial.printf("[BUTTON] Click %d/3\n", _clickCount);

                if (_clickCount >= 3) {
                    Serial.println("[SYSTEM] Reset triggered! Wiping NVS...");
                    StorageManager::wipe();
                    delay(1000);
                    ESP.restart();
                }
            }
        }
    }
    lastState = reading;
}