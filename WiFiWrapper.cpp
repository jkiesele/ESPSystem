#include "WiFiWrapper.h"


void WiFiWrapper::begin() {
    wifiShouldBeActive=true;
    Serial.println("Initializing WiFi...");
    WiFi.mode(WIFI_STA);  // Only connect to router, no AP mode
    WiFi.persistent(false);  // Prevent storing credentials
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        delay(200);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi Connection Failed!");
    }
    configureLowPowerMode();  // Apply low power settings
}

void WiFiWrapper::stop(){ 
    WiFi.disconnect(); 
    WiFi.mode(WIFI_OFF);
    wifiShouldBeActive=false;
}

void WiFiWrapper::loop() { 
    if (!wifiShouldBeActive) return;
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

void WiFiWrapper::disconnect() {
    WiFi.disconnect();
    Serial.println("WiFi Disconnected");
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
