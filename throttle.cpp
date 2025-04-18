#include "throttle.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp32-hal.h"
#include "Arduino.h"



int throttleCPU(float temperature, float cpu_temp_min, float cpu_temp_max) {

    // CPU throttling based on temperature
    static int currentCpuFrequency = CPU_MAX_FREQ;

    if(temperature < -99) {
        temperature = temperatureRead();
    }

    float scale = 1.0 - ((temperature - cpu_temp_min) / (cpu_temp_max - cpu_temp_min));  
    int newFreq = CPU_MIN_FREQ + (scale * (CPU_MAX_FREQ - CPU_MIN_FREQ));  

    newFreq = constrain(newFreq, CPU_MIN_FREQ, CPU_MAX_FREQ);

    //do this in steps, if change is not at least 10MHz, don't do it
    if(abs(newFreq - currentCpuFrequency) < 10){
        return currentCpuFrequency;
    }
    setCpuFrequencyMhz(newFreq);
    currentCpuFrequency = newFreq;
    return currentCpuFrequency;
}