#ifndef INCLUDED_PE32HUD_TEMPERATURESENSORCOMPONENT_H
#define INCLUDED_PE32HUD_TEMPERATURESENSORCOMPONENT_H

#include "pe32hud.h"

class DHTesp;

class TemperatureSensorComponent {
private:
    static constexpr unsigned long m_interval = 30000;
    unsigned long m_lastact;
    // FIXME: use SimpleKalmanFilter here (and eco2)
    DHTesp* m_dht11;

    const uint8_t m_pin_dht11;

public:
    TemperatureSensorComponent(uint8_t pin_dht11);

    void setup();
    void loop();

private:
    void sample();
};

#endif //INCLUDED_PE32HUD_TEMPERATURESENSORCOMPONENT_H
