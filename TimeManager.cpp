#include <TimeManager.h>
#include <LoggingBase.h>

// Helper: Convert a struct tm in UTC to time_t.
// Use timegm() if available. On some systems you might need to implement your own.
#ifndef timegm
  #define timegm mktime  // Note: mktime assumes local time, so ideally use timegm.
#endif

void TimeManager::begin() {
    timeClient.begin();
    syncTime();

    BaseType_t ok =  xTaskCreatePinnedToCore(
        &_syncTask,           // task function
        "TimeSync",           // name
        4*1024,               // stack size (bytes)
        this,                 // parameter = our TimeManager*
        tskIDLE_PRIORITY+1,   // priority
        &_syncTaskHandle,     // handle
        0                     // run on core 1 (or 0)
      );
    if (ok != pdPASS) {
        gLogger->println("Failed to create TimeSync task");
    }
}

void TimeManager::_syncTask(void* pvParameters){
    auto self = static_cast<TimeManager*>(pvParameters);
  
    const TickType_t delayTicks = pdMS_TO_TICKS(60UL*60UL*1000UL);  // one hour
  
    for(;;){
      vTaskDelay(delayTicks);
  
      // protect the NTP client
      if(xSemaphoreTake(self->_lock, pdMS_TO_TICKS(500)) == pdTRUE){
        self->syncTime();
        xSemaphoreGive(self->_lock);
      } else {
        // if we cannot take the lock, skip this round
        gLogger->println("TimeSync: failed to acquire lock");
      }
    }
  }

void TimeManager::syncTime() {
    if (WiFi.status() == WL_CONNECTED) {
        
        timeClient.setTimeOffset(0); // Ensure we start from UTC
        timeClient.update();
        
        uint32_t utcTime = timeClient.getEpochTime(); // Get raw UTC time
        struct tm timeinfo;
        gmtime_r((time_t*)&utcTime, &timeinfo);

        // Determine if Berlin is in summer time
        bool summerTime = isSummerTime(utcTime);
        timeOffset = summerTime ? 7200 : 3600;  // 2 hours for summer, 1 hour for winter

        // Apply DST offset
        time_t localTime = utcTime + timeOffset;


        struct timeval tv;
        tv.tv_sec = localTime;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        timeClient.setTimeOffset(timeOffset); 
    } else {
        gLogger->println("WiFi not connected, cannot sync time!");
    }
}


bool TimeManager::isSummerTime(uint32_t rawTime) const {
    struct tm timeInfo;
    gmtime_r((time_t*)&rawTime, &timeInfo);
    int year = timeInfo.tm_year + 1900;
    int month = timeInfo.tm_mon + 1;
    int day = timeInfo.tm_mday;
    
    // Outside DST months in Europe:
    if (month < 3 || month > 10)
        return false;
    if (month > 3 && month < 10)
        return true;
    
    // For March and October, calculate the last Sunday of the month.
    int lastSunday = 0;
    if (month == 3) {
        // March always has 31 days.
        struct tm t31 = {0};
        t31.tm_year = year - 1900;
        t31.tm_mon  = 2;    // March (0-indexed)
        t31.tm_mday = 31;
        t31.tm_hour = 12;   // Use midday to avoid edge issues
        time_t ts31 = timegm(&t31); // Get Unix time for March 31 in UTC
        // Calculate weekday: 0=Sunday, ... 6=Saturday.
        int wday31 = ((ts31 / 86400UL) + 4) % 7;  // Unix epoch (Jan 1, 1970) was a Thursday (4)
        lastSunday = 31 - ((wday31 + 6) % 7);
        // DST starts on the last Sunday of March.
        return day >= lastSunday;
    }
    if (month == 10) {
        // October always has 31 days.
        struct tm t31 = {0};
        t31.tm_year = year - 1900;
        t31.tm_mon  = 9;    // October (0-indexed)
        t31.tm_mday = 31;
        t31.tm_hour = 12;
        time_t ts31 = timegm(&t31);
        int wday31 = ((ts31 / 86400UL) + 4) % 7;
        lastSunday = 31 - ((wday31 + 6) % 7);
        // DST ends on the last Sunday of October: if current day is before that, DST is still active.
        return day < lastSunday;
    }
    return false;
}

String TimeManager::formattedDateAndTime(uint32_t rawTime){

    struct tm timeInfo;
    gmtime_r((time_t*)&rawTime, &timeInfo);  // Convert Unix time to UTC struct

    // Format the date as DD-MM-YYYY HH:MM:SS
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d %02d:%02d:%02d",
    timeInfo.tm_mday, timeInfo.tm_mon + 1, timeInfo.tm_year + 1900,
    timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

    return String(buffer);
}

String  TimeManager::getFormattedDateAndTime(uint32_t rawTime) const {
    return formattedDateAndTime(rawTime); // Call the static method to format the date and time
}
