#ifndef WIFI_WRAPPER_H
#define WIFI_WRAPPER_H

#include <WiFi.h>
#include "esp_wifi.h"
#include "Scheduler.h"

class WiFiWrapper {
private:
    const char* ssid;
    const char* password;
    bool wifiShouldBeActive = true;
    unsigned long lastWakeTime = 0;
    const unsigned long wakeDuration = 30000;  // Keep WiFi awake for 5s after activity
    Scheduler scheduler, reconnectScheduler;

public:
    WiFiWrapper(const char* ssid, const char* password)
        : ssid(ssid), password(password) {}

    void begin();
    void resume(){ begin(); }
    //after stop begin() must be called again to recover
    void stop();
    void loop() { scheduler.loop(); reconnectScheduler.loop(); }
    void disconnect();
    void checkAndReconnect();
    void configureLowPowerMode();
    void configureNormalPowerMode(){}//nothing here
    void keepWiFiAwake();

    unsigned long timeToNextTask() const { return scheduler.timeToNextTask(); }
};

#endif
