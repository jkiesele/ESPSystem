#ifndef TEMPERATURE_SAFETY_MANAGER_H
#define TEMPERATURE_SAFETY_MANAGER_H

#include <Arduino.h>

class WiFiWrapper;

// Configurable temperature thresholds
static constexpr float TEMP_SHUTDOWN = 100.0;
static constexpr float TEMP_DISABLE_WIFI = 95.0;
static constexpr float TEMP_REDUCE_WIFI_POWER = 90.0;


// Hysteresis threshold for restoring settings
static constexpr float TEMP_RESTORE_WIFI_POWER = 85.0;
static constexpr float TEMP_RESTORE_WIFI = 90.0;

class TemperatureSafetyManager {
private:
    bool wifiDisabled = false;
    bool lowPowerMode = false;
    bool shutdownTriggered = false;
    int currentCpuFrequency = 240;
    uint32_t runcount;
    
    WiFiWrapper* wifi;

public:
    TemperatureSafetyManager(WiFiWrapper* wifi=0): runcount(0),wifi(wifi) {} 
    
    void manageTemperatureSafety();
    void begin(){manageTemperatureSafety();}//execute directly
    void loop(uint32_t runevery=1000){
        if(runcount == runevery){
            runcount = 0;
            manageTemperatureSafety();
        }
        runcount++;
    }//just a wrapper for the main loop

    // Getters for external monitoring
    bool isWifiDisabled() const { return wifiDisabled; }
    bool isLowPowerMode() const { return lowPowerMode; }
    bool isShutdownTriggered() const { return shutdownTriggered; }
    int getCurrentCpuFrequency() const { return currentCpuFrequency; }
};

#endif // TEMPERATURE_SAFETY_MANAGER_H