#include "TemperatureSafetyManager.h"
#include <WiFi.h>
#include "esp_wifi.h"
#include "throttle.h"

#ifdef TEMP_SAFETY_USES_WEBLOG
#include "Log.h"
#else //MOCK class
class WebLog {
public:
    void addToLog(const String& message) { /* Mock implementation */ }
} webLog;
#endif

#include "WiFiWrapper.h"


void TemperatureSafetyManager::manageTemperatureSafety() {
    float temp = temperatureRead(); // Read internal temperature


    // ===========================
    // Emergency Shutdown Logic
    // ===========================
    if (temp >= TEMP_SHUTDOWN) {
        esp_sleep_enable_timer_wakeup(30ULL * 60 * 1000000);  // Wake up in 30 minutes and re-check
        esp_deep_sleep_start(); // Enter deep sleep (reset required)
        return;  // Exit early if shutting down
    }

    // ===========================
    // Reducing System Load
    // ===========================
    
    if (temp >= TEMP_DISABLE_WIFI && !wifiDisabled && wifi) {
        wifi->stop();  // Turn off WiFi completely
        wifiDisabled = true;
        webLog.addToLog("WiFi disabled due to high temperature.");
    } 

    if (temp >= TEMP_REDUCE_WIFI_POWER && !lowPowerMode && wifi) {
        wifi->configureLowPowerMode();  // Lower WiFi TX power
        lowPowerMode = true;
        webLog.addToLog("WiFi power reduced due to high temperature.");
    }

    //this can be silent
    int cpuf = throttleCPU(temp); //external
    currentCpuFrequency = cpuf;
    
    // ===========================
    // Restoring System State
    // ===========================

    if (temp <= TEMP_RESTORE_WIFI_POWER && lowPowerMode && wifi) {
        wifi->configureNormalPowerMode();  // Restore full WiFi power
        lowPowerMode = false;
        webLog.addToLog("WiFi power restored due to lower temperature.");
    }

    if (temp <= TEMP_RESTORE_WIFI && wifiDisabled && wifi) {
        wifi->resume();  // Turn WiFi back on
        wifiDisabled = false;
        webLog.addToLog("WiFi restored due to lower temperature.");
    }
}