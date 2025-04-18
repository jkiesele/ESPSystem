#ifndef TIME_MANAGER_H  // Fixed header guard
#define TIME_MANAGER_H

#include <WiFi.h>
#include "NTPClient.h"
#include "esp_wifi.h"
#include <WiFiUdp.h>
#include <time.h>
#include <TimeProviderBase.h>

static const uint32_t SECOND = 1000;  // 1 second = 1000 ms
static const uint32_t MINUTE = 60 * SECOND;  // 60,000 ms
static const uint32_t HOUR   = 60 * MINUTE;  // 3,600,000 ms
static const uint32_t DAY    = 24 * HOUR;    // 86,400,000 ms

class TimeManager: public TimeProviderBase {
private:
    WiFiUDP ntpUDP;
    NTPClient timeClient;
    int32_t timeOffset = 0; // Start with UTC (no offset)

    bool isSummerTime(uint32_t rawTime) const; //in UTC

public:
    TimeManager() : timeClient(ntpUDP, "pool.ntp.org", 0) {}

    void begin();
    void syncTime();

    int getHour() {
        return timeClient.getHours();
    }

    bool isNightTime() {
        int hour = getHour();
        return (hour >= 22 || hour < 7);
    }

    int getSecondsOfDay() {
        return timeClient.getHours() * 3600 + timeClient.getMinutes() * 60 + timeClient.getSeconds();
    }

    uint32_t getUnixTime() {
        return timeClient.getEpochTime(); 
    }

    uint32_t getUnixUTCTime(uint32_t localTime=0) {
        if(!localTime)
            return timeClient.getEpochTime() - timeOffset; // Always return UTC
        return localTime - timeOffset;
    }

    String getFormattedTime() {
        return timeClient.getFormattedTime();
    }
    
    String getFormattedDateAndTime(uint32_t rawTime) const;
    static  String formattedDateAndTime(uint32_t rawTime);

};

#endif  // TIME_MANAGER_H