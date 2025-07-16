#ifndef TIME_MANAGER_H  // Fixed header guard
#define TIME_MANAGER_H

#include <WiFi.h>
#include "NTPClient.h"
#include "esp_wifi.h"
#include <WiFiUdp.h>
#include <time.h>
#include <TimeProviderBase.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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

    SemaphoreHandle_t   _lock;         // protects timeClient / timeOffset
    TaskHandle_t        _syncTaskHandle;

    static void        _syncTask(void* pvParameters);
    void syncTime();//now private as it requires a lock

public:
    TimeManager() : timeClient(ntpUDP, "pool.ntp.org", 0) {
        _lock = xSemaphoreCreateMutex();
        _syncTaskHandle = nullptr;
    }
    ~TimeManager(){
        if(_syncTaskHandle){
          vTaskDelete(_syncTaskHandle);
        }
        if(_lock){
          vSemaphoreDelete(_lock);
        }
    }

    void begin();
    void loop() {
        //empty
    }
    bool isSynced() const {
        if(xSemaphoreTake(_lock, pdMS_TO_TICKS(50))==pdTRUE){
            bool synced = timeClient.isTimeSet();
            xSemaphoreGive(_lock);
            return synced;
        }
        return false; // if we cannot take the lock, assume not synced
    }   

    int getHours() {
        int h = 0;
        if(xSemaphoreTake(_lock, pdMS_TO_TICKS(50))==pdTRUE){
            h = timeClient.getHours();
            xSemaphoreGive(_lock);
        }
        return h;
    }
    int getHour(){return getHours();} // for compatibility with other classes
    int getMinutes() {
        int m = 0;
        if(xSemaphoreTake(_lock, pdMS_TO_TICKS(50))==pdTRUE){
            m = timeClient.getMinutes();
            xSemaphoreGive(_lock);
        }
        return m;
    }
    int getSeconds() {
        int s = 0;
        if(xSemaphoreTake(_lock, pdMS_TO_TICKS(50))==pdTRUE){
            s = timeClient.getSeconds();
            xSemaphoreGive(_lock);
        }
        return s;
    }

    uint32_t getDay(uint32_t rawTime=0) const {
        if(rawTime == 0)
            rawTime = timeClient.getEpochTime();
        return (((rawTime  / 86400L) + 4 ) % 7);
    }

    uint32_t getEpochTime() {
        uint32_t rawTime = 0;
        if(xSemaphoreTake(_lock, pdMS_TO_TICKS(50))==pdTRUE){
            rawTime = timeClient.getEpochTime();
            xSemaphoreGive(_lock);
        }
        return rawTime;
    }

    bool isNightTime() {
        int hour = getHour();
        return (hour >= 22 || hour < 7);
    }

    int getSecondsOfDay() {
        return getHours() * 3600 +  getMinutes() * 60 +  getSeconds();
    }

    uint32_t getUnixTime() {
        return  getEpochTime(); 
    }

    uint32_t getUnixUTCTime(uint32_t localTime=0) {
        if(!localTime)
            return  getEpochTime() - timeOffset; // Always return UTC
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
        String formattedTime;
        if(xSemaphoreTake(_lock, pdMS_TO_TICKS(50))==pdTRUE){
            formattedTime = timeClient.getFormattedTime();
            xSemaphoreGive(_lock);
        }
        return formattedTime;
    }
    
    String getFormattedDateAndTime(uint32_t rawTime) const;
    static  String formattedDateAndTime(uint32_t rawTime);

};

#endif  // TIME_MANAGER_H