#include "NetworkComponent.h"

#include "Device.h"

extern Device Device;

NetworkComponent::NetworkComponent()
#ifdef HAVE_ESP8266WIFI
    : m_wifistatus(WL_DISCONNECTED), m_mqttclient(m_mqttbackend)
#endif
{
}

void NetworkComponent::setup()
{
    Device.set_guid(String("EUI48:") + WiFi.macAddress());
    Device.set_alert(Device::INACTIVE_WIFI);
    m_wifidowntime = millis();
#ifdef HAVE_ESP8266WIFI
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);         // false is default, we don't need to save to flash
    WiFi.setAutoReconnect(false);   // we don't need this, we do it manually?
    handle_wifi_state_change(WL_IDLE_STATUS);
    m_wifistatus = WL_IDLE_STATUS;
    m_wifidowntime = m_lastact = millis();
    // Do not forget setId(). Some MQTT daemons will reject id-less connections.
    m_mqttclient.setId(String(Device.get_guid()).substring(0, 23));
#endif
}

void NetworkComponent::loop()
{
#ifdef HAVE_ESP8266WIFI
    wl_status_t wifistatus;
    if ((millis() - m_lastact) >= 3000 && (
                (wifistatus = WiFi.status()) != WL_CONNECTED || wifistatus != m_wifistatus)) {
        if (m_wifistatus != wifistatus) {
            handle_wifi_state_change(wifistatus);
            m_wifistatus = wifistatus;
            // Don't set m_lastact. We'll want to run WL_CONNECTED code below.
        } else if (m_wifistatus != WL_CONNECTED && (millis() - m_wifidowntime) > 5000) {
            wifistatus = WL_IDLE_STATUS;
            handle_wifi_state_change(wifistatus);
            m_wifistatus = wifistatus;
            m_lastact = millis();
        }
    }
#endif
    if (m_wifistatus == WL_CONNECTED && (millis() - m_lastact) >= m_interval) {
        const unsigned char *bssid = WiFi.BSSID();
        Serial << F("NetworkComponent: RSSI: ") << WiFi.RSSI() << F(", BSSID: 0x");
        Serial.print(bssid[0], HEX);
        Serial.print(bssid[1], HEX);
        Serial.print(bssid[2], HEX);
        Serial.print(bssid[3], HEX);
        Serial.print(bssid[4], HEX);
        Serial.print(bssid[5], HEX);
        Serial << F("\r\n");
        ensure_mqtt();
        sample();
        m_lastact = millis();  // after poll, so we don't hammer on failure
    }
}

void NetworkComponent::push_remote(String topic, String formdata)
{
#if 1
    if (m_mqttclient.connected()) {
        Serial << F("NetworkComponent: push: ") << topic << F(" :: ")
            << F("device_id=") << Device.get_guid() << F("&") << formdata << F("\r\n");
        m_mqttclient.beginMessage(topic);
        m_mqttclient.print(F("device_id="));
        m_mqttclient.print(Device.get_guid());
        m_mqttclient.print(F("&"));
        m_mqttclient.print(formdata);
        m_mqttclient.endMessage();
    }
#endif
}

#ifdef HAVE_ESP8266WIFI
void NetworkComponent::handle_wifi_state_change(wl_status_t wifistatus)
{
    // FIXME: translate wifistatus from number to something readable
    Serial << F("NetworkComponent: Wifi state ") << m_wifistatus << F(" -> ") << wifistatus << F("\r\n");

    if (m_wifistatus == WL_CONNECTED) {
        m_wifidowntime = millis();
    }
    String downtime(String("") + ((millis() - m_wifidowntime) / 1000) + " downtime");

    switch (wifistatus) {
        case WL_IDLE_STATUS:
            Device.set_alert(Device::INACTIVE_WIFI);
            Device.set_error(F("Wifi connecting"), downtime);
            WiFi.disconnect(true, true);
#ifdef SECRET_WIFI_BSSID
            // Speed up wifi connect, especially for poor (<= -70 RSSI) connections.
            if ((millis() - m_wifidowntime) < 30000) {
                const uint8_t bssid[6] = SECRET_WIFI_BSSID;
                WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS, 0, bssid, true);
                Serial << F("NetworkComponent: Wifi connecting (with preset BSSID)...\r\n");
            } else {
                WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
                Serial << F("NetworkComponent: Wifi connecting...\r\n");
            }
#else
            WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
            Serial << F("NetworkComponent: Wifi connecting...\r\n");
#endif
            break;
        case WL_CONNECTED:
            Device.clear_alert(Device::INACTIVE_WIFI);
            break;
        case WL_NO_SSID_AVAIL:
        case WL_CONNECT_FAILED:
        case WL_DISCONNECTED:
            Device.set_alert(Device::INACTIVE_WIFI);
            Device.set_error(String(F("Wifi state ")) + wifistatus, downtime);
            break;
#ifdef WL_CONNECT_WRONG_PASSWORD
        case WL_CONNECT_WRONG_PASSWORD:
            Device.set_alert(Device::INACTIVE_WIFI);
            Device.set_error(F("Wifi wrong creds."), downtime);
            break;
#endif
        default:
            Device.set_alert(Device::INACTIVE_WIFI);
            Device.set_error(String(F("Wifi unknown ")) + wifistatus, downtime);
            break;
    }
#ifdef DEBUG
    Serial << F("  --NetworkComponent: Wifi values BEGIN\r\n");
    WiFi.printDiag(Serial);  // FIXME/XXX: beware, shows password on serial output
    Serial << F("  --NetworkComponent: Wifi values END\r\n");
#endif
}
#endif

void NetworkComponent::ensure_mqtt()
{
    m_mqttclient.poll();
    if (!m_mqttclient.connected()) {
        if (m_mqttclient.connect(SECRET_MQTT_BROKER, SECRET_MQTT_PORT)) {
            Serial << F("NetworkComponent: MQTT connected to " SECRET_MQTT_BROKER "\r\n");
        } else {
            Serial << F("NetworkComponent: MQTT connection to " SECRET_MQTT_BROKER " failed: ")
                << m_mqttclient.connectError() << F("\r\n");
        }
    }
}

void NetworkComponent::sample()
{
#ifdef DEBUG
    Serial << F("  --NetworkComponent: fetch/update\r\n");
#endif
    String remote_packet = fetch_remote();
    if (remote_packet.length()) {
        RemoteResult res;
        parse_remote(remote_packet, res);
        handle_remote(res);
    }
}

String NetworkComponent::fetch_remote()
{
    String payload;
#ifdef HAVE_ESP8266HTTPCLIENT
    HTTPClient http;
    http.begin(m_httpbackend, SECRET_HUD_URL);
    int http_code = http.GET();
    if (http_code >= 200 && http_code < 300) {
        // Fetch data and truncate just in case.
        payload = http.getString().substring(0, 512);
    } else {
        Device.set_error(String(F("HTTP/")) + http_code, F("(error)"));
    }
    http.end();
#endif
    return payload;
}

void NetworkComponent::parse_remote(const String& remote_packet, RemoteResult& res)
{
    int start = 0;
    bool done = false;

    res.color = Device::COLOR_YELLOW;

    while (!done) {
        String line;
        int lf = remote_packet.indexOf('\n', start);
        if (lf < 0) {
            line = remote_packet.substring(start);
            done = true;
        } else {
            line = remote_packet.substring(start, lf);
            start = lf + 1;
        }

        if (line.startsWith("color:#")) {
            res.color = strtol(line.c_str() + 7, NULL, 16);
        } else if (line.startsWith("line0:")) {
            res.message0 = line.substring(6); //, 6 + LCD_COLS);
        } else if (line.startsWith("line1:")) {
            res.message1 = line.substring(6); //, 6 + LCD_COLS);
        } else if (line.startsWith("action:UP")) {
            res.sunscreen = Device::ACTION_SUNSCREEN_UP;
        } else if (line.startsWith("action:RESET")) {
            res.sunscreen = Device::ACTION_SUNSCREEN_NONE;
        } else if (line.startsWith("action:DOWN")) {
            res.sunscreen = Device::ACTION_SUNSCREEN_DOWN;
        }
    }
}

void NetworkComponent::handle_remote(const RemoteResult& res)
{
    Device.set_text(res.message0, res.message1, res.color);
    Device.add_action(res.sunscreen);
}
