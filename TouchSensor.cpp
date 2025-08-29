#include "TouchSensor.h"
#include "driver/touch_pad.h"
#include "threadSafeArduino.h"

TouchSensor::TouchSensor(uint8_t pin, uint16_t threshold, uint16_t hysteresis,  uint8_t samples)
    : pin_(pin), threshold_(threshold), hysteresis_(hysteresis), state_(false), lastValue_(0), samples_(samples)
{
}

void TouchSensor::update() {
    auto thisvalue = threadSafe::touchRead(pin_);
    lastValue_ = thisvalue;

    if (state_) {
        if (lastValue_ < (threshold_ - hysteresis_) && sampleCount > 0) {
            sampleCount--;
        }
    } else {
        if (lastValue_ > (threshold_ + hysteresis_) && sampleCount < samples_) {
            sampleCount++;
        }
    }

    if (sampleCount >= samples_) {
        state_ = true;
    } else if (sampleCount == 0) {
        state_ = false;
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