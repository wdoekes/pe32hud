#ifndef INCLUDED_PE32HUD_SUNSCREENCOMPONENT_H
#define INCLUDED_PE32HUD_SUNSCREENCOMPONENT_H

#include "pe32hud.h"

class SunscreenComponent {
private:
    static constexpr unsigned long m_interval = 600;  // 0.6 sec
    unsigned long m_lastact;
    enum state {
        DEPRESSED = 0,
        REQUEST = 1,
        BUTTON_SEL = 2,
        REQUEST_SEL = 3,
        BUTTON_DN = 4,
        REQUEST_DN = 5,
        BUTTON_UP = 6,
        REQUEST_UP = 7
    } m_state;

    const uint8_t m_somfy_sel;
    const uint8_t m_somfy_dn;
    const uint8_t m_somfy_up;

public:
    SunscreenComponent(uint8_t pin_select, uint8_t pin_down, uint8_t pin_up);

    void setup();
    void loop();

    void press_select() {
        m_state = REQUEST_SEL;
    }
    void press_down() {
        m_state = REQUEST_DN;
    }
    void press_up() {
        m_state = REQUEST_UP;
    }

private:
    void press_at_most_one(enum state st);
    void handle_press_request();
    void handle_depress();
};

#endif //INCLUDED_PE32HUD_SUNSCREENCOMPONENT_H
