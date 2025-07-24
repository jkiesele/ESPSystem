#pragma once
#include <Arduino.h>

class TouchSensor {
public:
    TouchSensor(uint8_t pin, uint16_t threshold, uint16_t hysteresis = 500);

    void update();  // Call this in loop()
    bool isActive() const;

    void begin() {
        pinMode(pin_, INPUT);
    }

    void setThreshold(uint16_t threshold);
    void setHysteresis(uint16_t hysteresis);

    uint16_t threshold() const;
    uint16_t hysteresis() const;
    uint16_t lastValue() const;

private:
    uint8_t pin_;
    uint16_t threshold_;
    uint16_t hysteresis_;
    bool state_;
    uint16_t lastValue_;
};