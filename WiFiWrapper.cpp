#include "WiFiWrapper.h"
#include <LoggingBase.h>


void WiFiWrapper::begin(bool connectToNetwork,  bool lowPowerMode) {
    
    gLogger->println("Initializing WiFi...");
    WiFi.mode(WIFI_STA);  // Only connect to router, no AP mode
    WiFi.persistent(false);  // Prevent storing credentials
    if(lowPowerMode)
        configureLowPowerMode(); 
    if(connectToNetwork)
        connect() ;
}

void WiFiWrapper::connect(){
    wifiShouldBeConnected=true;
    WiFi.begin(ssid, password);

    gLogger->print("Connecting to WiFi");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        delay(200);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        gLogger->print("\nWiFi Connected! ");
        gLogger->print("IP Address: ");
        gLogger->println(WiFi.localIP());
    } else {
        gLogger->println("\nWiFi Connection Failed!");
    }
}

void WiFiWrapper::disconnect(){
    WiFi.disconnect(); 
    wifiShouldBeConnected=false;
}
void WiFiWrapper::stop(){ 
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
    if(autoSleep && ! WiFi.getSleep() && (now - sleepTimerStartedAt) > wakeDuration){
        WiFi.setSleep(true);
    } 
}

void WiFiWrapper::checkAndReconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost! Attempting to reconnect...");
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
