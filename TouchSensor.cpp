#include "TouchSensor.h"

TouchSensor::TouchSensor(uint8_t pin, uint16_t threshold, uint16_t hysteresis)
    : pin_(pin), threshold_(threshold), hysteresis_(hysteresis), state_(false), lastValue_(0)
{
}

void TouchSensor::update() {
    lastValue_ = touchRead(pin_);

    if (!state_) {
        if (lastValue_ < (threshold_ - hysteresis_)) {
            state_ = true;
        }
    } else {
        if (lastValue_ > (threshold_ + hysteresis_)) {
            state_ = false;
        }
    }
}

bool TouchSensor::isActive() const {
    return state_;
}

void TouchSensor::setThreshold(uint16_t threshold) {
    threshold_ = threshold;
}

void TouchSensor::setHysteresis(uint16_t hysteresis) {
    hysteresis_ = hysteresis;
}

uint16_t TouchSensor::threshold() const {
    return threshold_;
}

uint16_t TouchSensor::hysteresis() const {
    return hysteresis_;
}

uint16_t TouchSensor::lastValue() const {
    return lastValue_;
}