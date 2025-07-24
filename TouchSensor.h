#pragma once
#include <Arduino.h>

class TouchSensor {
public:
    TouchSensor(uint8_t pin, uint16_t threshold, uint16_t hysteresis = 50);

    void update();  // Call this in loop()
    bool isActive() const;

    void setThreshold(uint16_t threshold);
    void setHysteresis(uint16_t hysteresis);

    uint16_t getThreshold() const;
    uint16_t getHysteresis() const;
    uint16_t getLastValue() const;

private:
    uint8_t pin_;
    uint16_t threshold_;
    uint16_t hysteresis_;
    bool state_;
    uint16_t lastValue_;
};