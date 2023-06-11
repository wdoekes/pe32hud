#include "TemperatureSensorComponent.h"

#include <DHTesp.h>        // DHT_sensor_library_for_ESPx

#include "Device.h"

extern Device Device;

TemperatureSensorComponent::TemperatureSensorComponent(uint8_t pin_dht11) :
    m_dht11(new DHTesp), m_pin_dht11(pin_dht11)
{
}

void TemperatureSensorComponent::setup() {
    Device.set_alert(Device::INACTIVE_DHT11);
    m_dht11->setup(m_pin_dht11, DHTesp::DHT11);
    m_lastact = (millis() - m_interval);
    Device.clear_alert(Device::INACTIVE_DHT11);
}

void TemperatureSensorComponent::loop() {
    if ((millis() - m_lastact) >= m_interval) {
#ifdef DEBUG
        Serial << F("  --TemperatureSensorComponent: sample\r\n");
#endif
        m_lastact = millis();
        sample();
    }
}

void TemperatureSensorComponent::sample() {
    float humidity = m_dht11->getHumidity();
    float temperature = m_dht11->getTemperature();

    // Print values
    Serial << F("DHT11:  ") <<                         // (comment for Arduino IDE)
        m_dht11->getStatusString() << F(" status,  ") <<  // "OK"
        temperature << F(" 'C,  ") <<                    // (comment for Arduino IDE)
        humidity << F(" phi(RH)\r\n");                   // (comment for Arduino IDE)

    // Publish values
    Device.publish("pe32/hud/temp/xwwwform", (
        String("status=") + m_dht11->getStatusString() +
        String("&temperature=") + temperature +
        String("&humidity=") + humidity));
}
