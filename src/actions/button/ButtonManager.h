#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

class ButtonManager {
public:
    static void begin(int pin);
    static void update();
private:
    static int _pin;
    static int _clickCount;
    static unsigned long _lastClickTime;
};
#endif