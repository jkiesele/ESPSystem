#include "WiFiWrapper.h"
#include <LoggingBase.h>


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
        gLogger->println(WiFi.localIP());
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
    if( !alwaysOn &&
        autoSleep && 
        ! WiFi.getSleep() && 
        (now - sleepTimerStartedAt) > wakeDuration){
        WiFi.setSleep(true);
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

void WiFiWrapper::configureLowPowerMode(bool enable) {
    if(enable){
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // Enable power-saving mode
        esp_wifi_set_max_tx_power(8);  // Reduce TX power to save energy
        WiFi.setSleep(true);  // Allow WiFi sleep when idle
    }
    else{
        esp_wifi_set_ps(WIFI_PS_NONE);  // Disable power-saving mode
        esp_wifi_set_max_tx_power(20);  // Set TX power to maximum
        WiFi.setSleep(false);  // Disable WiFi sleep
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
    WiFi.setSleep(false);
    sleepTimerStartedAt = millis();
}

void  WiFiWrapper::stayUp(){
    autoSleep = false;
    WiFi.setSleep(false);
    sleepTimerStartedAt = millis(); 
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