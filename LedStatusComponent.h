#ifndef INCLUDED_PE32HUD_LEDSTATUSCOMPONENT_H
#define INCLUDED_PE32HUD_LEDSTATUSCOMPONENT_H

#include "pe32hud.h"

static constexpr int LED_ON = LOW;
static constexpr int LED_OFF = HIGH;

class LedStatusComponent {
public:
    enum blinkmode {
        NO_BLINK = -1,
        BLINK_NORMAL = 0,
        BLINK_BOOT = 1,
        BLINK_WIFI = 2,
        BLINK_DHT11 = 3,
        BLINK_CCS811 = 4,
        BLINK_SUNSCREEN = 5,
    };

private:
    enum blinkmode m_blinkmode;
    const int8_t m_blinktimes[6][14] = {
        // 100=red_on(100ms), -100=red_off(100ms), 0=stop
        {10, 0,},                                                       // BLINK_NORMAL (no blue)
        {100, 0,},                                                      // BLINK_BOOT
        {100, 100, 100, -100, 100, 0,},                                 // BLINK_WIFI   "wiii-fi"
        {100, -100, 100, -100, 100, 0,},                                // BLINK_DHT11  "d-h-t"
        {100, -100, 100, 100, 100, -100, 100, 0},                       // BLINK_CCS811 "c-ooo-2"
        {50, -50, 50, -50, 50, -50, 50, -50, 50, -50, 50, -50, 50, 0}   // BLINK_SUNSCREEN
    };
    const int8_t* m_blinktime;
    unsigned long m_lastact;

    const uint8_t m_led_red;
    const uint8_t m_led_blue;

public:
    LedStatusComponent(uint8_t led_red, uint8_t led_blue)
        : m_led_red(led_red), m_led_blue(led_blue) {}

    void setup() {
        // Blue led ON during boot (or errors). Red can show stuff whenever.
        pinMode(m_led_red, OUTPUT);
        pinMode(m_led_blue, OUTPUT);
    }

    void loop() {
        // Not doing anything?
        if (m_blinktime == NULL) {
            if (m_blinkmode != NO_BLINK) {
                // Start blinking.
                m_blinktime = m_blinktimes[m_blinkmode];
                //printf("lastact %lu (+%lu) val %hhd S\n", m_lastact, (millis() - m_lastact), *m_blinktime);
                digitalWrite(m_led_blue, m_blinkmode == BLINK_NORMAL ? LED_OFF : LED_ON);
                digitalWrite(m_led_red, *m_blinktime > 0 ? LED_ON : LED_OFF);
                m_lastact = millis();
            }
            return;
        }

        // The current value is not 0 but -100 or 100.
        if (*m_blinktime) {
            uint8_t abs_time = (*m_blinktime >= 0 ? *m_blinktime : -*m_blinktime);
            if ((millis() - m_lastact) >= abs_time) {
                m_blinktime++;
                //printf("lastact %lu (+%lu) val %hhd\n", m_lastact, (millis() - m_lastact), *m_blinktime);
                digitalWrite(m_led_red, *m_blinktime > 0 ? LED_ON : LED_OFF);
                m_lastact = millis();
            }
            // The current value is 0 and we've waited for a second.
        } else if ((millis() - m_lastact) >= 1000) {
            if (m_blinkmode != NO_BLINK) {
                // Restart blinking.
                m_blinktime = m_blinktimes[m_blinkmode];
                //printf("lastact %lu (+%lu) val %hhd R\n", m_lastact, (millis() - m_lastact), *m_blinktime);
                digitalWrite(m_led_red, *m_blinktime > 0 ? LED_ON : LED_OFF);
            } else {
                // Stop blinking.
                m_blinktime = NULL;
                digitalWrite(m_led_blue, LED_OFF);
                digitalWrite(m_led_red, LED_OFF);
            }
            m_lastact = millis();
        }
    }

    void set_blink(enum blinkmode bm) {
        if (bm != m_blinkmode) {
            Serial << F("LedStatusComponent: switching blinkmode to ") << bm << F("\r\n");
            m_blinkmode = bm;
        }
    }
};

#endif //INCLUDED_PE32HUD_LEDSTATUSCOMPONENT_H
