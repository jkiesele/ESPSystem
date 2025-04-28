#ifndef WIFI_WRAPPER_H
#define WIFI_WRAPPER_H

#include <WiFi.h>
#include "esp_wifi.h"

class WiFiWrapper {
private:
    const char* ssid;
    const char* password;
    bool wifiShouldBeConnected = true;
    bool autoSleep = true;
    unsigned long lastWakeTime = 0;
    static constexpr uint32_t wakeDuration = 30000;  // Keep WiFi awake for 30s after activity
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

    // Returns current RSSI in dBm (negative value; e.g., -40 is strong, -90 is weak)
    // If not connected, returns -127
    int32_t getSignalStrength() const;
    SignalLevel classifySignalLevel(int32_t rssi) const;


    //pings and keep awake for another 30 seconds
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
