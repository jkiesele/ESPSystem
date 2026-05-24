#include "TouchSensor.h"
#include "driver/touch_pad.h"
#include "threadSafeArduino.h"

TouchSensor::TouchSensor(uint8_t pin, uint16_t threshold, uint16_t hysteresis,  uint8_t samples, uint8_t nMovingAvg,
int referencePin)
    : pin_(pin), threshold_(threshold), hysteresis_(hysteresis), state_(false), 
    lastValue_(0), samples_(samples), nMovingAvg_(nMovingAvg), referencePin_(referencePin)
{
}

void TouchSensor::update() {
    auto thisvalue = threadSafe::touchRead(pin_);
    if(referencePin_ >= 0) {
        uint32_t referenceValue = threadSafe::touchRead(referencePin_);
        // add a simple fixed offset to keep things uint16_t and avoid underflow
        thisvalue = ((uint32_t)thisvalue + 10000) - referenceValue;
    }
    if(thisvalue == lastValue_)
        return; // no change => do nothing we might be calling over sampling frequency
    
    //use nMovingAvg_ if >0
    uint16_t n = nMovingAvg_;
    lastValue_ = (n * lastValue_ + thisvalue) / (n + 1);

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