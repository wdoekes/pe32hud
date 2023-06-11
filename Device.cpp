#include "Device.h"

#include "DisplayComponent.h"
#include "LedStatusComponent.h"
#include "NetworkComponent.h"
#include "SunscreenComponent.h"

void Device::set_text(const String& msg0, const String& msg1, unsigned long color)
{
    m_displaycomponent->set_text(msg0, msg1, color);
}

void Device::set_error(const String& msg0, const String& msg1)
{
    m_displaycomponent->set_text(msg0, msg1, COLOR_YELLOW);
}

void Device::set_or_clear_alert(enum alert al, bool is_alert)
{
    if (is_alert) {
        m_alerts |= al;
    } else {
        m_alerts &= ~al;
    }
    if (m_alerts & NOTIFY_SUNSCREEN) {
        m_ledstatuscomponent->set_blink(LedStatusComponent::BLINK_SUNSCREEN);
    } else if (m_alerts & INACTIVE_WIFI) {
        m_ledstatuscomponent->set_blink(LedStatusComponent::BLINK_WIFI);
    } else if (m_alerts & INACTIVE_DHT11) {
        m_ledstatuscomponent->set_blink(LedStatusComponent::BLINK_DHT11);
    } else if (m_alerts & INACTIVE_CCS811) {
        m_ledstatuscomponent->set_blink(LedStatusComponent::BLINK_CCS811);
    } else if (m_alerts) {
        // What problems?
        m_ledstatuscomponent->set_blink(LedStatusComponent::BLINK_BOOT);
    } else {
        m_ledstatuscomponent->set_blink(LedStatusComponent::BLINK_NORMAL);
    }
}

void Device::add_action(enum action atn)
{
    if (atn & ACTION_SUNSCREEN) {
        if (m_lastsunscreen != atn) {
            switch (atn) {
                case ACTION_SUNSCREEN_SELECT:
                    m_sunscreencomponent->press_select();
                    break;
                case ACTION_SUNSCREEN_DOWN:
                    m_sunscreencomponent->press_down();
                    break;
                case ACTION_SUNSCREEN_UP:
                    m_sunscreencomponent->press_up();
                    break;
                default:
                    break;
            }
            m_lastsunscreen = atn;
        }
    }
}

void Device::publish(const String& topic, const String& formdata)
{
    m_networkcomponent->push_remote(topic, formdata);
}
