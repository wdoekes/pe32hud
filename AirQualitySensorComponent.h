#ifndef INCLUDED_PE32HUD_AIRQUALITYSENSORCOMPONENT_H
#define INCLUDED_PE32HUD_AIRQUALITYSENSORCOMPONENT_H

#include "pe32hud.h"

class Adafruit_CCS811;

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

    Adafruit_CCS811* m_ccs811;
    TwoWire* m_wire;
    BinToggle& m_reset;

public:
    AirQualitySensorComponent(TwoWire* theWire = &Wire, BinToggle& reset = NullToggle);

    void setup();
    void loop();

private:
    void dump_info();
    void sample();
};

#endif //INCLUDED_PE32HUD_AIRQUALITYSENSORCOMPONENT_H
