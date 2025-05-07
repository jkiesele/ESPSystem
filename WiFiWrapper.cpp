#include "WiFiWrapper.h"
#include <LoggingBase.h>


static WiFiWrapper* instance = nullptr;


static void wifiStatusComplete(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(10));  // wait 10ms for hardware to adjust
    instance->_setStateReady(true);
    vTaskDelete(NULL);  
}


WiFiWrapper::WiFiWrapper(const char* ssid, const char* password)
: ssid(ssid), password(password) {
    if (instance != nullptr) {
        gLogger->println("WiFiWrapper instance already exists. Only one instance is allowed.");
        abort();
    }
    instance = this;
    autoSleep = true;
    alwaysOn = false;
}

void WiFiWrapper::begin(bool connectToNetwork,  bool lowPowerMode) {
    
    gLogger->println("Initializing WiFi...");
    WiFi.mode(WIFI_STA);  // Only connect to router, no AP mode
    WiFi.persistent(false);  // Prevent storing credentials

    if(connectToNetwork)
        connect() ;
    if(lowPowerMode)
        configureLowPowerMode(); 
    else
        configureFullPowerMode();
    setTXPower(8); // Start with 8 dBm
    sleepTimerStartedAt  = millis();
    lastReconnectAttempt = millis();
}

bool WiFiWrapper::connect(){
    wifiShouldBeConnected=true;
    WiFi.begin(ssid, password);

    gLogger->print("Connecting to WiFi");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        delay(200);
        gLogger->print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        gLogger->print("\nWiFi Connected! ");
        gLogger->print("IP Address: ");
        gLogger->println(WiFi.localIP().toString());
        return true;
    } else {
        gLogger->println("\nWiFi Connection Failed!");
        return false;
    }
}

void WiFiWrapper::disconnect(){
    if(alwaysOn){
        gLogger->println("WiFiWrapper::disconnect() called, but alwaysOn is set. Ignoring disconnect.");
         return;
    }
    WiFi.disconnect(); 
    wifiShouldBeConnected=false;
}
void WiFiWrapper::stop(){ 
    if(alwaysOn){
        gLogger->println("WiFiWrapper::stop() called, but alwaysOn is set. Ignoring stop.");
         return;
    }
    disconnect();
    WiFi.mode(WIFI_OFF);
}

void WiFiWrapper::loop() { 
    if (!wifiShouldBeConnected) return;
    //check the timers manually here
    uint32_t now = millis();
    if(now - lastReconnectAttempt > reconnectInterval){
        lastReconnectAttempt = now;
        checkAndReconnect();
    }
    if(alwaysOn)
        return; // no need to check sleep state
    if( autoSleep && 
        ! WiFi.getSleep() && 
        (now - sleepTimerStartedAt) > wakeDuration){
        configureLowPowerMode();
    } 
}

void WiFiWrapper::checkAndReconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        gLogger->println("WiFi lost! Attempting to reconnect...");
        WiFi.disconnect();
        delay(100);
        WiFi.begin(ssid, password); 
    }
}


void WiFiWrapper::configureFullPowerMode() {
    if(currentPowerState == PowerState::Full) return; // already in full power mode
    esp_wifi_set_ps(WIFI_PS_NONE);
    stateReady = false;
    // Arduino wrapper call (keeps the wrapper’s internal flag in sync)
    WiFi.setSleep(false);
    currentPowerState = PowerState::Full;
    sleepTimerStartedAt = millis();   // reset your idle timer here, too
    //if lastReconnectAttempt would reconnect immediately, reset it so it waits for 5 s at least
    if(millis() - lastReconnectAttempt > reconnectInterval){
        lastReconnectAttempt = millis() - reconnectInterval + 5000;
    }
    BaseType_t res = xTaskCreatePinnedToCore(wifiStatusComplete, "WiFiReady", 1024, nullptr, 0, nullptr, tskNO_AFFINITY);
    if (res != pdPASS) {
        gLogger->println("[WiFiWrapper] Failed to create WiFiReady task");
    }
}

void WiFiWrapper::configureLowPowerMode() {
    if(currentPowerState == PowerState::Low) return; // already in low power mode
    stateReady = false;
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    WiFi.setSleep(true);
    currentPowerState = PowerState::Low;
    BaseType_t res = xTaskCreatePinnedToCore(wifiStatusComplete, "WiFiReady", 1024, nullptr, 0, nullptr, tskNO_AFFINITY);
    if (res != pdPASS) {
        gLogger->println("[WiFiWrapper] Failed to create WiFiReady task");
    }
}

int32_t WiFiWrapper::getSignalStrength() const {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    } else {
        return -127; // convention: -1 dBm = not connected
    }
}

WiFiWrapper::SignalLevel WiFiWrapper::classifySignalLevel(int32_t rssi) const {
    if (rssi >= -50) {
        return SignalLevel::EXCELLENT;
    } else if (rssi >= -60) {
        return SignalLevel::GOOD;
    } else if (rssi >= -70) {
        return SignalLevel::FAIR;
    } else if (rssi >= -80) {
        return SignalLevel::POOR;
    } else if (rssi > -127) {
        return SignalLevel::VERY_POOR;
    } else {
        return SignalLevel::DISCONNECTED;
    }
}

void WiFiWrapper::keepWiFiAwake() {
   configureFullPowerMode();
   sleepTimerStartedAt = millis();
}

void  WiFiWrapper::stayUp(){
    configureFullPowerMode();
    autoSleep = false;
}
void  WiFiWrapper::backToAutoSleep(){
    autoSleep = true;
}

void WiFiWrapper::setAlwaysOn(bool set) {
    alwaysOn = set;
    if(alwaysOn){
        gLogger->println("WiFiWrapper::alwaysOn set; Ignoring disconnects etc from here on an staying in connected full power mode");
        connect();
        stayUp();
        configureFullPowerMode();
    }
    else{
        gLogger->println("WiFiWrapper::setAlwaysOn() turned off. Re-init the state you want to be in manually.");
    }
}