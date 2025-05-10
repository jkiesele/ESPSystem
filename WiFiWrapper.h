#ifndef WIFI_WRAPPER_H
#define WIFI_WRAPPER_H

#include <WiFi.h>
#include "esp_wifi.h"
#include "LoggingBase.h"
#include <atomic>

class WiFiWrapper {
private:
    const char* ssid;
    const char* password;
    bool wifiShouldBeConnected = true;
    bool autoSleep = true;
    uint32_t wakeDuration = 30000;  // Keep WiFi awake for 30s after activity
    static constexpr uint32_t reconnectInterval = 60000; // 1 minute
    
    uint32_t lastReconnectAttempt = 0;
    uint32_t sleepTimerStartedAt = 0;

    bool alwaysOn = false;

    enum class PowerState { Full, Low };
    PowerState currentPowerState = PowerState::Full;  // or Low if you start that way
    std::atomic<bool> stateReady{false}; // for thread safety
    SemaphoreHandle_t stateMutex{nullptr};
    
public:
    enum class SignalLevel {
        EXCELLENT,
        GOOD,
        FAIR,
        POOR,
        VERY_POOR,
        DISCONNECTED
    };

    WiFiWrapper(const char* ssid, const char* password);

    ~WiFiWrapper(){
        if (stateMutex) {
            vSemaphoreDelete(stateMutex);
        }
    }
    //called outside of threads
    void begin(bool connectToNetwork=true, bool lowPowerMode=true, bool locked=true);

    bool connect(bool locked=true);
    void resume(bool locked=true);
    void disconnect(bool locked=true);
    //after stop begin() must be called again to recover
    void stop(bool locked=true);
    //call at least every few seconds, no need for fast polling
    void loop();
    void checkAndReconnect(bool locked=true);
    void configureLowPowerMode(bool locked=true);
    void configureNormalPowerMode(bool locked=true){ configureLowPowerMode(locked);} //compatibility
    void configureFullPowerMode(bool locked=true);

    inline bool isStateReady()const{
        return stateReady.load(std::memory_order_acquire);
    }
    
    void setTXPower(uint8_t power, bool locked=true);

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

    // internal for tasks, do not call directly
    inline void _setStateReady(bool ready) {
        stateReady.store(ready, std::memory_order_release);
    }
};

#endif
