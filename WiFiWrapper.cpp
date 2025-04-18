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
    reconnectScheduler.stop();
    reconnectScheduler.addTimedTask([this]() {
        checkAndReconnect();
    }, 
    5*60*1000, //delay 5 minutes
    true,  //repeat
    5*60*1000);//repeat every 5 minutes 

    configureLowPowerMode();  // Apply low power settings
}

void WiFiWrapper::stop(){ 
    WiFi.disconnect(); 
    WiFi.mode(WIFI_OFF);
    wifiShouldBeActive=false;
    reconnectScheduler.stop();
    scheduler.stop();
}

void WiFiWrapper::disconnect() {
    WiFi.disconnect();
    Serial.println("WiFi Disconnected");
}

void WiFiWrapper::checkAndReconnect() {
    Serial.println("Checking WiFi connection...");
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost! Attempting to reconnect...");
        WiFi.disconnect();
        delay(100);
        WiFi.begin(ssid, password); 
    }
}

void WiFiWrapper::configureLowPowerMode() {
    Serial.println("Configuring Low Power Mode...");
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // Enable power-saving mode
    esp_wifi_set_max_tx_power(8);  // Reduce TX power to save energy
    WiFi.setSleep(true);  // Allow WiFi sleep when idle
}

void WiFiWrapper::keepWiFiAwake() {
    Serial.println("Extending WiFi wake time...");
    lastWakeTime = millis();
    
    if (WiFi.getSleep()) {  // Only disable sleep if it's currently enabled
        WiFi.setSleep(false);
    }

    //remove previously pending tasks next loop
    scheduler.stop();
    scheduler.addTimedTask([this]() { WiFi.setSleep(true); }, 
    wakeDuration, //delay
    false);//don't repeat
}

