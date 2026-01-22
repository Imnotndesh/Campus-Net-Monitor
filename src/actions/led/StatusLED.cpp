#include "StatusLED.h"

int StatusLED::_pin;
SystemStatus StatusLED::_currentStatus = MODE_PORTAL;
unsigned long StatusLED::_lastAction = 0;
int StatusLED::_pulseCount = 0;
bool StatusLED::_isPaused = false;

void StatusLED::begin(int pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
}

void StatusLED::setStatus(SystemStatus status) {
    if (_currentStatus != status) {
        _currentStatus = status;
        _pulseCount = 0;
        _isPaused = false;
    }
}

void StatusLED::update() {
    if (_currentStatus == STATUS_OK) {
        digitalWrite(_pin, HIGH);
        return;
    }

    unsigned long now = millis();
    
    if (_isPaused) {
        if (now - _lastAction >= 2000) {
            _isPaused = false;
            _pulseCount = 0;
            _lastAction = now;
        }
        return;
    }

    if (now - _lastAction >= 200) {
        _lastAction = now;
        bool ledState = digitalRead(_pin);
        digitalWrite(_pin, !ledState);

        if (ledState == HIGH) { // We just finished one full blink (ON then OFF)
            _pulseCount++;
            if (_pulseCount >= (int)_currentStatus) {
                _isPaused = true;
                digitalWrite(_pin, LOW);
            }
        }
    }
}