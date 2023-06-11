#include "AirQualitySensorComponent.h"

#include "CCS811.h"
#include "Device.h"

extern Device Device;

AirQualitySensorComponent::AirQualitySensorComponent(uint8_t pin_sda, uint8_t pin_scl, uint8_t ccs811_rst) :
     m_ccs811(new CCS811),
     m_pin_sda(pin_sda),
     m_pin_scl(pin_scl),
     m_ccs811_rst(ccs811_rst)
{
}

void AirQualitySensorComponent::setup()
{
    Device.set_alert(Device::INACTIVE_CCS811);
    pinMode(m_ccs811_rst, OUTPUT);
    digitalWrite(m_ccs811_rst, HIGH);  // not reset
}

void AirQualitySensorComponent::loop()
{
    enum state new_state = STATE_NONE;

    switch (m_state) {
        case STATE_NONE:
            // Force reset, we _must_ call begin() after this.
            digitalWrite(m_ccs811_rst, LOW);
            new_state = STATE_RESETTING;
            break;
        case STATE_RESETTING:
            // Reset/wake pulses must be at least 20us, so 1ms is enough.
            if ((millis() - m_lastact) <= 1) {
                return;
            }
            digitalWrite(m_ccs811_rst, HIGH);
            new_state = STATE_WAKING;
            break;
        case STATE_WAKING:
            // At 20ms after boot/reset are we up again.
            if ((millis() - m_lastact) <= 20) {
                return;
            }
            if (m_ccs811->begin()) {  // addr 90/0x5A
                Device.clear_alert(Device::INACTIVE_CCS811);
                new_state = STATE_ACTIVE;
                dump_info();
                sample();
            } else {
                Serial << F("ERROR: CCS811 communication failure\r\n");
                Device.set_alert(Device::INACTIVE_CCS811);
                new_state = STATE_FAILING;
            }
            break;
        case STATE_ACTIVE:
            // Sample every m_interval seconds, normally.
            if ((millis() - m_lastact) < m_interval) {
                return;
            }
            new_state = STATE_ACTIVE;  // keep same state
            sample();
            break;
        case STATE_FAILING:
            // Wait a while if we failed to start.
            if ((millis() - m_lastact) < m_interval) {
                return;
            }
            new_state = STATE_NONE;
            break;
    }
#ifdef DEBUG
    Serial << F("  --AirQualitySensorComponent: state ") <<  // (idefix)
        m_state << F(" -> ") << new_state << F("\r\n");
#endif
    m_state = new_state;
    m_lastact = millis();
}

void AirQualitySensorComponent::dump_info()
{
    // Print CCS811 sensor information
    // FIXME: improved Serial << usage
    Serial.println(F("CCS811 Sensor Enabled:"));
    Serial.print(F("Hardware ID:           0x"));
    Serial.println(m_ccs811->getHWID(), HEX);
    Serial.print(F("Hardware Version:      0x"));
    Serial.println(m_ccs811->getHWVersion(), HEX);
    Serial.print(F("Firmware Boot Version: 0x"));
    Serial.println(m_ccs811->getFWBootVersion(), HEX);
    Serial.print(F("Firmware App Version:  0x"));
    Serial.println(m_ccs811->getFWAppVersion(), HEX);
    Serial.println();
}

void AirQualitySensorComponent::sample()
{
    // FIXME: use SimpleKalmanFilter here (and for DHT11)
    uint16_t ccs_eco2;  // CCS811 eCO2
    uint16_t ccs_tvoc;  // CCS811 TVOC
    uint8_t ccs_error;  // CCS811 error register

    // Read the sensor data, this updates multiple fields
    // in the CCS811 library
    m_ccs811->readAlgResultDataRegister();

    // Read error register if an error was reported
    if (m_ccs811->hasERROR()) {
        // FIXME: improved Serial << usage
        Serial.println(F("ERROR: CCS811 Error Flag Set"));

        ccs_error = m_ccs811->getERROR_ID();
        Serial.print(F("CCS811 Error Register = "));
        Serial.println(ccs_error);
        Serial.println();
        Device.set_alert(Device::INACTIVE_CCS811);
        m_state = STATE_FAILING;
        return;
    }

    // Data is ready
    if (m_ccs811->isDATA_READY()) {
        ccs_eco2 = m_ccs811->geteCO2();
        ccs_tvoc = m_ccs811->getTVOC();

        // Verify eCO2 is valid
        if (ccs_eco2 > CCS811_ECO2_MAX) {
            Serial.print(F(" (ERROR: CCS811 eCO2 Exceeded Limit) "));
        }

        // Print eCO2 to serial monitor
        Serial << F("CCS811: ") << ccs_eco2 << F(" ppm(eCO2),  ");

        // Verify TVOC is valid
        if (ccs_tvoc > CCS811_TVOC_MAX) {
            Serial.print(F(" (ERROR: CCS811 TVOC Exceeded Limit) "));
        }

        // Print TVOC to serial monitor
        Serial << ccs_tvoc << F(" ppb(TVOC)\n");

        // Publish values
        Device.publish("pe32/hud/co2/xwwwform", (
                    String("eco2=") + ccs_eco2 + String("&tvoc=") + ccs_tvoc));
    } else {
        Serial.print(F(" (ERROR: CCS811 Data Not Ready)\n"));
    }
}
