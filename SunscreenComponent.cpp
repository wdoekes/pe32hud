#include "SunscreenComponent.h"

#include "Device.h"

extern Device Device;

SunscreenComponent::SunscreenComponent(uint8_t pin_select, uint8_t pin_down, uint8_t pin_up)
        : m_somfy_sel(pin_select), m_somfy_dn(pin_down), m_somfy_up(pin_up)
{
    // Run this _before_ setup time. Otherwise we might press buttons before setup is called.
    // However, we're pushing buttons while flashing the device, unfortunately.
    pinMode(m_somfy_sel, OUTPUT);
    pinMode(m_somfy_dn, OUTPUT);
    pinMode(m_somfy_up, OUTPUT);
    digitalWrite(m_somfy_sel, HIGH);
    digitalWrite(m_somfy_dn, HIGH);
    digitalWrite(m_somfy_up, HIGH);
}

void SunscreenComponent::setup() {
    Device.clear_alert(Device::NOTIFY_SUNSCREEN);
}

void SunscreenComponent::loop() {
    if (m_state == DEPRESSED) {
        ;
    } else if (m_state & REQUEST) {
        handle_press_request();
    } else if ((millis() - m_lastact) >= m_interval) {
        handle_depress();
    }
}

void SunscreenComponent::press_at_most_one(enum state st)
{
    digitalWrite(m_somfy_sel, st == REQUEST_SEL ? LOW : HIGH);
    digitalWrite(m_somfy_dn, st == REQUEST_DN ? LOW : HIGH);
    digitalWrite(m_somfy_up, st == REQUEST_UP ? LOW : HIGH);
    m_lastact = millis();
}

void SunscreenComponent::handle_press_request() {
    Device.set_alert(Device::NOTIFY_SUNSCREEN);
    // TODO: something with flickering/blinking?
    // lcd.setColor(COLOR_YELLOW)?
#ifdef DEBUG
    Serial << F("  --SunscreenComponent: pressing ") << m_state << F("\r\n");
#endif
    press_at_most_one(m_state);
    m_state = static_cast<enum state>(m_state & ~REQUEST);
}

void SunscreenComponent::handle_depress() {
    Device.clear_alert(Device::NOTIFY_SUNSCREEN);
#ifdef DEBUG
    Serial << F("  --SunscreenComponent: depressing ") << m_state << F("\r\n");
#endif
    press_at_most_one(DEPRESSED);
    m_state = DEPRESSED;
}
