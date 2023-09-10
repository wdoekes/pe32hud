#ifndef INCLUDED_LOCAL_BOGODUINO_ARDUINOMQTTCLIENT_H
#define INCLUDED_LOCAL_BOGODUINO_ARDUINOMQTTCLIENT_H

/* This requires WiFi includes. But they are handled elsewhere, we hope. */

struct MqttClient {
    MqttClient(WiFiClient& wifi_client) {}

    void setId(const String& id) {}

    bool connect(const String& host, uint16_t port) { return true; }
    void poll() {}
    bool connected() const { return true; }
    const char* connectError() { return "some error"; }

    void beginMessage(const String& topic) {}
    void print(const String& message) {}
    void endMessage() {}
};

#endif //INCLUDED_LOCAL_BOGODUINO_ARDUINOMQTTCLIENT_H

