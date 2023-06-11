#ifndef INCLUDED_PE32HUD_NETWORKCOMPONENT_H
#define INCLUDED_PE32HUD_NETWORKCOMPONENT_H

#include "pe32hud.h"

#include "Device.h"

class NetworkComponent {
#ifdef TEST_BUILD
    friend int main(int argc, char** argv);
#endif

public:
    struct RemoteResult {
        String message0;
        String message1;
        unsigned long color;
        enum Device::action sunscreen;
    };

private:
    static constexpr unsigned long m_interval = 5000;
    unsigned long m_lastact;
    unsigned long m_wifidowntime;
#ifdef HAVE_ESP8266WIFI
    wl_status_t m_wifistatus;
    // NOTE: We need a WiFiClient for _each_ component that does network
    // connections (httpclient and mqttclient), otherwise using one will
    // disconnect the TCP session of the other.
    // See also: https://forum.arduino.cc/t/simultaneous-mqtt-and-http-post/430361/7
    WiFiClient m_httpbackend;
    WiFiClient m_mqttbackend;
    MqttClient m_mqttclient;
#endif

public:
    NetworkComponent();

    void setup();
    void loop();

    void push_remote(String topic, String formdata);

private:
#ifdef HAVE_ESP8266WIFI
    void handle_wifi_state_change(wl_status_t wifistatus);
#endif

    void ensure_mqtt();
    void sample();

    String fetch_remote();

    static void parse_remote(const String& remote_packet, RemoteResult& res);
    void handle_remote(const RemoteResult& res);
};

#endif //INCLUDED_PE32HUD_NETWORKCOMPONENT_H
