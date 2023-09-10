#ifndef INCLUDED_LOCAL_BOGODUINO_ESPWIFI_H
#define INCLUDED_LOCAL_BOGODUINO_ESPWIFI_H

#include <Serial.h>

/* ESP8266WiFiType.h */
typedef enum WiFiMode {
    WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3
} WiFiMode_t;

/* wl_definitions.h */
typedef enum {
    WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_WRONG_PASSWORD   = 6,
    WL_DISCONNECTED     = 7
} wl_status_t;

struct WiFiClient {
    wl_status_t status() { return WL_CONNECTED; }
    void mode(WiFiMode_t mode) {}
    void persistent(bool value) {}
    void setAutoReconnect(bool value) {}

    void begin(const String &ssid, const String &password, int32_t channel = 0, const uint8_t* bssid = NULL, bool connect = true) {}
    void disconnect(bool val1, bool val2) {}
    uint8_t waitForConnectResult(unsigned long delay = 60000) { return WL_CONNECTED; }
    char const* macAddress() const { return "11:22:33:44:55:66"; }
    unsigned char const* const BSSID() const {
	static unsigned char const buf[6] = {0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee};
	return buf; }
    int32_t RSSI() { return -64; }

    void printDiag(Print &p) {}
};

extern WiFiClient WiFi;

#endif //INCLUDED_LOCAL_BOGODUINO_ESPWIFI_H
