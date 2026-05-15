#include "WiFiWrapper.h"
#include <LoggingBase.h>
#include "esp_task_wdt.h"  


static WiFiWrapper* instance = nullptr;

struct LockGuard {
    SemaphoreHandle_t m;
    LockGuard(SemaphoreHandle_t m_ = nullptr): m(m_) {
      if (m) xSemaphoreTake(m, portMAX_DELAY);
    }
    ~LockGuard() {
      if (m) xSemaphoreGive(m);
    }
  };

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
    stateReady = true;
    stateMutex = xSemaphoreCreateMutex();
    if (stateMutex == nullptr) {
        gLogger->println("WiFiWrapper: Failed to create mutex");
        abort();
    }
}

WiFiWrapper::APChoice WiFiWrapper::findBestAPForSSID(bool locked) {
    LockGuard lg(locked ? stateMutex : nullptr);

    APChoice best;

    gLogger->println("[WiFiWrapper] Scanning for APs with configured SSID...");

    // Synchronous scan.
    // second argument true = include hidden networks. Harmless for normal SSIDs.
    int n = WiFi.scanNetworks(false, true);

    if (n <= 0) {
        gLogger->println("[WiFiWrapper] No WiFi networks found during scan.");
        WiFi.scanDelete();
        return best;
    }

    String targetSSID(ssid);

    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) != targetSSID) {
            continue;
        }

        const uint8_t* bssidPtr = WiFi.BSSID(i);
        if (!bssidPtr) {
            continue;
        }

        int32_t rssi = WiFi.RSSI(i);
        int32_t channel = WiFi.channel(i);

        gLogger->print("[WiFiWrapper] Candidate ");
        gLogger->print(WiFi.BSSIDstr(i));
        gLogger->print(" ch=");
        gLogger->print(channel);
        gLogger->print(" RSSI=");
        gLogger->println(rssi);

        if (!best.valid || rssi > best.rssi) {
            best.valid = true;
            best.rssi = rssi;
            best.channel = channel;
            memcpy(best.bssid, bssidPtr, 6);
        }
    }

    WiFi.scanDelete();

    if (!best.valid) {
        gLogger->println("[WiFiWrapper] No AP found for configured SSID.");
        return best;
    }

    char bssidStr[18];
    snprintf(
        bssidStr,
        sizeof(bssidStr),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        best.bssid[0], best.bssid[1], best.bssid[2],
        best.bssid[3], best.bssid[4], best.bssid[5]
    );

    gLogger->print("[WiFiWrapper] Best AP: ");
    gLogger->print(bssidStr);
    gLogger->print(" ch=");
    gLogger->print(best.channel);
    gLogger->print(" RSSI=");
    gLogger->println(best.rssi);

    return best;
}


bool WiFiWrapper::connectToSpecificAP(const APChoice& ap, bool locked) {
    LockGuard lg(locked ? stateMutex : nullptr);

    if (!ap.valid) {
        return false;
    }

    gLogger->println("[WiFiWrapper] Connecting to selected BSSID...");

    WiFi.disconnect(false, false);
    vTaskDelay(pdMS_TO_TICKS(200));

    WiFi.begin(ssid, password, ap.channel, ap.bssid);

    gLogger->print("[WiFiWrapper] Connecting");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        gLogger->print(".");
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(200));
        attempt++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        gLogger->println("\n[WiFiWrapper] BSSID-pinned connection failed.");
        return false;
    }

    gLogger->print("\n[WiFiWrapper] Connected to selected AP. IP: ");
    gLogger->println(WiFi.localIP().toString());

    gLogger->print("[WiFiWrapper] Connected BSSID: ");
    gLogger->println(WiFi.BSSIDstr());

    gLogger->print("[WiFiWrapper] RSSI: ");
    gLogger->println(WiFi.RSSI());

    return true;
}


bool WiFiWrapper::connectToBestAvailableAP(bool locked) {
    LockGuard lg(locked ? stateMutex : nullptr);

    APChoice best = findBestAPForSSID(false);

    if (!best.valid) {
        return false;
    }

    return connectToSpecificAP(best, false);
}


bool WiFiWrapper::maybeRoamToBetterAP(bool locked) {
    LockGuard lg(locked ? stateMutex : nullptr);

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    int32_t currentRSSI = WiFi.RSSI();

    if (currentRSSI > roamScanThreshold) {
        return false;
    }

    gLogger->print("[WiFiWrapper] Current RSSI is weak: ");
    gLogger->println(currentRSSI);

    APChoice best = findBestAPForSSID(false);

    if (!best.valid) {
        return false;
    }

    bool candidateClearlyBetter =
        best.rssi >= currentRSSI + roamDeltaThreshold;

    bool currentVeryBadAndCandidateGood =
        currentRSSI < -75 && best.rssi > -68;

    if (!candidateClearlyBetter && !currentVeryBadAndCandidateGood) {
        gLogger->println("[WiFiWrapper] No sufficiently better AP found. Staying connected.");
        return false;
    }

    gLogger->print("[WiFiWrapper] Roaming from RSSI ");
    gLogger->print(currentRSSI);
    gLogger->print(" to candidate RSSI ");
    gLogger->println(best.rssi);

    return connectToSpecificAP(best, false);
}

void WiFiWrapper::begin(bool connectToNetwork, bool lowPowerMode, bool locked) {
    LockGuard lg(locked ? stateMutex : nullptr);
    gLogger->println("Initializing WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);

    if (connectToNetwork)
        connect(false);

    if (lowPowerMode)
        configureLowPowerMode(false); 
    else
        configureFullPowerMode(false);

    sleepTimerStartedAt  = millis();
    lastReconnectAttempt = millis();
    lastRoamCheck        = millis();
}

void WiFiWrapper::resume(bool locked) {
    if (stateReady.load(std::memory_order_acquire)) {
        gLogger->println("WiFiWrapper::resume() called, but state not ready. Ignoring resume.");
        return;
    }
    begin(locked);
}

bool WiFiWrapper::connect(bool locked) {
    LockGuard lg(locked ? stateMutex : nullptr);

    wifiShouldBeConnected = true;

    bool connected = connectToBestAvailableAP(false);

    if (connected) {
        return true;
    }

    gLogger->println("[WiFiWrapper] Best-AP connection failed. Falling back to normal WiFi.begin().");

    WiFi.begin(ssid, password);

    gLogger->print("Connecting to WiFi");
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 30) {
        gLogger->print(".");
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(200));
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        gLogger->print("  WiFi Connected! ");
        gLogger->print("IP Address: ");
        gLogger->println(WiFi.localIP().toString());

        gLogger->print("[WiFiWrapper] Connected BSSID: ");
        gLogger->println(WiFi.BSSIDstr());

        gLogger->print("[WiFiWrapper] RSSI: ");
        gLogger->println(WiFi.RSSI());

        return true;
    }

    gLogger->println("\nWiFi Connection Failed!");
    return false;
}

void WiFiWrapper::disconnect(bool locked){
    LockGuard lg( locked ? stateMutex : nullptr );
    if(alwaysOn){
        gLogger->println("WiFiWrapper::disconnect() called, but alwaysOn is set. Ignoring disconnect.");
         return;
    }
    WiFi.disconnect(); 
    wifiShouldBeConnected=false;
}
void WiFiWrapper::stop(bool locked){ 
    LockGuard lg( locked ? stateMutex : nullptr );
    if(alwaysOn){
        gLogger->println("WiFiWrapper::stop() called, but alwaysOn is set. Ignoring stop.");
         return;
    }
    disconnect(false);//not locked again
    WiFi.mode(WIFI_OFF);
}

void WiFiWrapper::loop() { 
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    //read the state
    uint32_t tmp_now = millis();
    auto tmp_wifiShouldBeConnected = wifiShouldBeConnected;
    auto tmp_lastReconnectAttempt = lastReconnectAttempt;
    auto tmp_lastRoamCheck = lastRoamCheck;
    auto tmp_sleepTimerStartedAt = sleepTimerStartedAt;
    auto tmp_autoSleep = autoSleep;
    auto tmp_alwaysOn = alwaysOn;
    auto tmp_wakeDuration = wakeDuration;
    auto tmp_arduinoSleep = WiFi.getSleep();
    xSemaphoreGive(stateMutex);

    if (!tmp_wifiShouldBeConnected) return;

    if (tmp_now - tmp_lastReconnectAttempt > reconnectInterval) {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        lastReconnectAttempt = tmp_now;
        checkAndReconnect(false);
        xSemaphoreGive(stateMutex);
    }

    if (tmp_now - tmp_lastRoamCheck > roamCheckInterval) {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        lastRoamCheck = tmp_now;
        maybeRoamToBetterAP(false);
        xSemaphoreGive(stateMutex);
    }

    if (tmp_alwaysOn)
        return;
    if( tmp_wakeDuration > 0 && tmp_autoSleep && 
        ! tmp_arduinoSleep && 
        (tmp_now - tmp_sleepTimerStartedAt) > tmp_wakeDuration){
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        //yes, we could use locked=true, this is just to keep it parallel to the other calls here
        configureLowPowerMode(false); 
        xSemaphoreGive(stateMutex);
    } 
}

void WiFiWrapper::checkAndReconnect(bool locked) {
    LockGuard lg(locked ? stateMutex : nullptr);

    if (WiFi.status() != WL_CONNECTED) {
        gLogger->println("WiFi lost! Attempting to reconnect...");
        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        connect(false);
    }
}


void WiFiWrapper::configureFullPowerMode(bool locked) {
    LockGuard lg( locked ? stateMutex : nullptr );
    if(currentPowerState == PowerState::Full) return; // already in full power mode
    //the user should have checked this but as a safety
    while(!stateReady.load(std::memory_order_acquire)){
        vTaskDelay(pdMS_TO_TICKS(5));  
    }
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

void WiFiWrapper::configureLowPowerMode(bool locked) {
    LockGuard lg( locked ? stateMutex : nullptr );
    if(currentPowerState == PowerState::Low) return; // already in low power mode
    //the user should have checked this but as a safety
    while(!stateReady.load(std::memory_order_acquire)){
        vTaskDelay(pdMS_TO_TICKS(5));  
    }
    stateReady = false;
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    WiFi.setSleep(true);
    currentPowerState = PowerState::Low;
    BaseType_t res = xTaskCreatePinnedToCore(wifiStatusComplete, "WiFiReady", 1024, nullptr, 0, nullptr, tskNO_AFFINITY);
    if (res != pdPASS) {
        gLogger->println("[WiFiWrapper] Failed to create WiFiReady task");
    }
}

void WiFiWrapper::setTXPower(uint8_t power, bool locked){
    LockGuard lg( locked ? stateMutex : nullptr );
    //check if power is in range
    if (power < 0 || power > 20) {
        gLogger->println("Invalid TX power level. Must be between 0 and 20 dBm.");
        return;
    }
    //cast to wifi_power_t
    wifi_power_t wifiPower = static_cast<wifi_power_t>(power);
    WiFi.setTxPower(wifiPower);
}

int32_t WiFiWrapper::getSignalStrength() const {
    LockGuard lg(stateMutex);
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

uint8_t WiFiWrapper::getChannel() const {
    wifi_second_chan_t secondChannel;
    uint8_t channel;
    LockGuard lg(stateMutex);
    esp_wifi_get_channel(&channel, &secondChannel);
    return channel;
}

void WiFiWrapper::keepWiFiAwake() {
   LockGuard lg(stateMutex);
   configureFullPowerMode(false);
   sleepTimerStartedAt = millis();
}

void  WiFiWrapper::stayUp(){
    LockGuard lg(stateMutex);
    configureFullPowerMode(false);
    autoSleep = false;
}
void  WiFiWrapper::backToAutoSleep(){
    LockGuard lg(stateMutex);
    autoSleep = true;
}

void WiFiWrapper::setAlwaysOn(bool set) {
    LockGuard lg(stateMutex);
    alwaysOn = set;
    if(alwaysOn){
        gLogger->println("WiFiWrapper::alwaysOn set; Ignoring disconnects etc from here on an staying in connected full power mode");
        connect(false);
        autoSleep = false;
        configureFullPowerMode(false);
    }
    else{
        gLogger->println("WiFiWrapper::setAlwaysOn() turned off. Re-init the state you want to be in manually.");
    }
}

bool WiFiWrapper::isConnected() const {
    LockGuard lg(stateMutex);
    return WiFi.status() == WL_CONNECTED;
}

wl_status_t WiFiWrapper::getWiFiStatus() const {
    LockGuard lg(stateMutex);
    return WiFi.status();
}

String WiFiWrapper::getBSSID() const {
    LockGuard lg(stateMutex);

    if (WiFi.status() != WL_CONNECTED) {
        return String("-");
    }

    return WiFi.BSSIDstr();
}

String WiFiWrapper::getSSID() const {
    LockGuard lg(stateMutex);

    if (WiFi.status() != WL_CONNECTED) {
        return String("-");
    }

    return WiFi.SSID();
}

String WiFiWrapper::getLocalIP() const {
    LockGuard lg(stateMutex);

    if (WiFi.status() != WL_CONNECTED) {
        return String("-");
    }

    return WiFi.localIP().toString();
}

String WiFiWrapper::getGatewayIP() const {
    LockGuard lg(stateMutex);

    if (WiFi.status() != WL_CONNECTED) {
        return String("-");
    }

    return WiFi.gatewayIP().toString();
}

String WiFiWrapper::getHostname() const {
    LockGuard lg(stateMutex);

    const char* hostname = WiFi.getHostname();
    if (!hostname) {
        return String("-");
    }

    return String(hostname);
}

String WiFiWrapper::getConnectionSummary() const {
    LockGuard lg(stateMutex);

    String s;

    if (WiFi.status() != WL_CONNECTED) {
        s += "WiFi: disconnected";
        s += " | status: ";
        s += String(static_cast<int>(WiFi.status()));
        return s;
    }

    wifi_second_chan_t secondChannel;
    uint8_t channel = 0;
    esp_wifi_get_channel(&channel, &secondChannel);

    const char* hostname = WiFi.getHostname();

    s += "WiFi: connected";
    s += " | IP: ";
    s += WiFi.localIP().toString();
    s += " | host: ";
    s += hostname ? String(hostname) : String("-");
    s += " | SSID: ";
    s += WiFi.SSID();
    s += " | BSSID: ";
    s += WiFi.BSSIDstr();
    s += " | ch: ";
    s += String(channel);
    s += " | RSSI: ";
    s += String(WiFi.RSSI());
    s += " dBm";

    return s;
}