#ifndef INCLUDED_LOCAL_BOGODUINO_ESP8622WIFI_H
#define INCLUDED_LOCAL_BOGODUINO_ESP8622WIFI_H

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
    void begin(const String &ssid, const String &password) {}
    void mode(WiFiMode_t mode) {}
    uint8_t waitForConnectResult(unsigned long delay = 60000) { return WL_CONNECTED; }
    void printDiag(Print &p) {}
};

extern WiFiClient WiFi;

#endif //INCLUDED_LOCAL_BOGODUINO_ESP8622WIFI_H
