#ifndef INCLUDED_PE32HUD_AIRQUALITYSENSORCOMPONENT_H
#define INCLUDED_PE32HUD_AIRQUALITYSENSORCOMPONENT_H

#include "pe32hud.h"

class CCS811;

class AirQualitySensorComponent {
private:
    static constexpr unsigned long m_interval = 30000;  // 30s
    unsigned long m_lastact;
    enum state {
        STATE_NONE,
        STATE_RESETTING,
        STATE_WAKING,
        STATE_ACTIVE,
        STATE_FAILING
    } m_state;

    CCS811* m_ccs811;
    const uint8_t m_pin_sda;
    const uint8_t m_pin_scl;
    const uint8_t m_ccs811_rst;

public:
    AirQualitySensorComponent(uint8_t pin_sda, uint8_t pin_scl, uint8_t ccs811_rst);

    void setup();
    void loop();

private:
    void dump_info();
    void sample();
};

#endif //INCLUDED_PE32HUD_AIRQUALITYSENSORCOMPONENT_H
