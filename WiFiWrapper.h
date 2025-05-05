#ifndef WIFI_WRAPPER_H
#define WIFI_WRAPPER_H

#include <WiFi.h>
#include "esp_wifi.h"
#include "LoggingBase.h"

class WiFiWrapper {
private:
    const char* ssid;
    const char* password;
    bool wifiShouldBeConnected = true;
    bool autoSleep = true;
    unsigned long lastWakeTime = 0;
    uint32_t wakeDuration = 30000;  // Keep WiFi awake for 30s after activity
    static constexpr uint32_t reconnectInterval = 60000; // 1 minute
    
    uint32_t lastReconnectAttempt = 0;
    uint32_t sleepTimerStartedAt = 0;

    bool alwaysOn = false;

public:
    enum class SignalLevel {
        EXCELLENT,
        GOOD,
        FAIR,
        POOR,
        VERY_POOR,
        DISCONNECTED
    };

    WiFiWrapper(const char* ssid, const char* password)
        : ssid(ssid), password(password) {}

    void begin(bool connectToNetwork=true, bool lowPowerMode=true);
    bool connect();
    void resume(){ begin(); }
    void disconnect();
    //after stop begin() must be called again to recover
    void stop();
    //call at least every few seconds, no need for fast polling
    void loop();
    void checkAndReconnect();
    void configureLowPowerMode(bool enable=true);
    void configureFullPowerMode(){
        configureLowPowerMode(false);
    }//nothing here
    void configureNormalPowerMode(){//FIXME with a better implementation
        configureFullPowerMode();
    }//nothing here
    void setTXPower(uint8_t power){
        //check if power is in range
        if (power < 0 || power > 20) {
            gLogger->println("Invalid TX power level. Must be between 0 and 20 dBm.");
            return;
        }
        //cast to wifi_power_t
        wifi_power_t wifiPower = static_cast<wifi_power_t>(power);
        WiFi.setTxPower(wifiPower);
    }

    // Returns current RSSI in dBm (negative value; e.g., -40 is strong, -90 is weak)
    // If not connected, returns -127
    int32_t getSignalStrength() const;
    SignalLevel classifySignalLevel(int32_t rssi) const;

    void setWakeDuration(uint32_t duration) {
        wakeDuration = duration;
    }
    //pings and keep awake for another wakeDuration seconds
    void keepWiFiAwake();

    //wakes up the wifi if it was asleep, and keeps it awake unless auto sleep is enabled again
    void stayUp();
    //goes back to sleep only if device was in sleep before calling wakeUp; does not interfere with auto sleep if enabled
    //also does not enable auto sleep if it was not enabled
    void backToAutoSleep();

    // this is mostly for debugging, reject any state changes from on and connected, even if disconnect etc is called
    void setAlwaysOn(bool set);
};

#endif
