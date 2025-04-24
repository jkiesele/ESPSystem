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

    // returns the UNIX‑time (seconds since epoch) of the next occurrence
    // of HH:MM in 24h, (midnight = 00:00).
    uint32_t getUnixNextTimeWeHit(int hours, int minutes = 0) {
        static constexpr uint32_t SEC_PER_DAY = 24UL * 3600UL;
    
        uint32_t now       = getUnixTime();
        uint32_t elapsed   = now % SEC_PER_DAY;                    // seconds since today’s 00:00 UTC
        uint32_t targetOff = uint32_t(hours) * 3600UL              // seconds into the day
                            + uint32_t(minutes) * 60UL;
        uint32_t dayStart  = now - elapsed;                        // UNIX time at today’s 00:00 UTC
        uint32_t candidate = dayStart + targetOff;                 // today’s HH:MM
    
        // if that time has already passed (or is exactly now), move to tomorrow
        if (candidate <= now) {
            candidate += SEC_PER_DAY;
        }
        return candidate;
    }

    uint32_t getSecondsUntilWeHit(int hours, int minutes = 0){
        uint32_t now = getUnixTime();
        uint32_t target = getUnixNextTimeWeHit(hours, minutes);
        return target - now;
    }

    String getFormattedTime() {
        return timeClient.getFormattedTime();
    }
    
    String getFormattedDateAndTime(uint32_t rawTime) const;
    static  String formattedDateAndTime(uint32_t rawTime);

};

#endif  // TIME_MANAGER_H