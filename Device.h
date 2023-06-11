#ifndef INCLUDED_PE32HUD_DEVICE_H
#define INCLUDED_PE32HUD_DEVICE_H

#include "pe32hud.h"

class DisplayComponent;
class LedStatusComponent;
class NetworkComponent;
class SunscreenComponent;

class Device {
public:
    enum action {
        ACTION_SUNSCREEN = 0x7,
        ACTION_SUNSCREEN_NONE = 0x1,
        ACTION_SUNSCREEN_SELECT = 0x2,
        ACTION_SUNSCREEN_DOWN = 0x3,
        ACTION_SUNSCREEN_UP = 0x4
    };
    enum {
        COLOR_RED = 0xff0000,
        COLOR_YELLOW = 0xffff00,
        COLOR_GREEN = 0x00ff00,
        COLOR_BLUE = 0x0000ff
    };
    enum alert {
        BOOTING = 1,
        INACTIVE_WIFI = 2,
        INACTIVE_DHT11 = 4,
        INACTIVE_CCS811 = 8,
        NOTIFY_SUNSCREEN = 16
    };

private:
    /* We use the guid to store something unique to identify the device by.
     * For now, we'll populate it with the ESP8266 Wifi MAC address. */
    char m_guid[24]; // "EUI48:11:22:33:44:55:66"

    DisplayComponent* m_displaycomponent;
    LedStatusComponent* m_ledstatuscomponent;
    NetworkComponent* m_networkcomponent;
    SunscreenComponent* m_sunscreencomponent;

    enum action m_lastsunscreen;
    uint8_t m_alerts;

public:
    Device()
        : m_lastsunscreen(ACTION_SUNSCREEN_NONE) { memcpy(m_guid, "EUI48:11:22:33:44:55:66", 24); }

    void set_displaycomponent(DisplayComponent* displaycomponent) {
        m_displaycomponent = displaycomponent;
    }
    void set_ledstatuscomponent(LedStatusComponent* ledstatuscomponent) {
        m_ledstatuscomponent = ledstatuscomponent;
    }
    void set_networkcomponent(NetworkComponent* networkcomponent) {
        m_networkcomponent = networkcomponent;
    }
    void set_sunscreencomponent(SunscreenComponent* sunscreencomponent) {
        m_sunscreencomponent = sunscreencomponent;
    }

    const char* get_guid() { return m_guid; }
    void set_guid(const String& guid) { strncpy(m_guid, guid.c_str(), sizeof(m_guid) - 1); }

    void set_text(const String& msg0, const String& msg1, unsigned long color);
    void set_error(const String& msg0, const String& msg1);

    void set_alert(enum alert al) { set_or_clear_alert(al, true); }
    void clear_alert(enum alert al) { set_or_clear_alert(al, false); }

    void add_action(enum action atn);

    void publish(const String& topic, const String& formdata);

private:
    void set_or_clear_alert(enum alert al, bool is_alert);
};

#endif //INCLUDED_PE32HUD_DEVICE_H
