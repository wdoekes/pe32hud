#include "AirQualitySensorComponent.h"

#include <Adafruit_CCS811.h>

#include "Device.h"

#define CCS811_ECO2_MAX 8191 // stolen from elsewhere
#define CCS811_TVOC_MAX 1187 // stolen from elsewhere

extern Device Device;

AirQualitySensorComponent::AirQualitySensorComponent(TwoWire* theWire, BinToggle& reset) :
     m_ccs811(new Adafruit_CCS811),
     m_wire(theWire),
     m_reset(reset)
{
    m_reset.toggle(false); // disable/down reset pin
}

void AirQualitySensorComponent::setup()
{
    Device.set_alert(Device::INACTIVE_CCS811);
}

void AirQualitySensorComponent::loop()
{
    enum state new_state = STATE_NONE;

    switch (m_state) {
        case STATE_NONE:
            // Force reset, we _must_ call begin() after this.
            m_reset.toggle(true);
            new_state = STATE_RESETTING;
            break;
        case STATE_RESETTING:
            // Reset/wake pulses must be at least 20us, so 1ms is enough.
            if ((millis() - m_lastact) <= 1) {
                return;
            }
            m_reset.toggle(false);
            new_state = STATE_WAKING;
            break;
        case STATE_WAKING:
            // At 20ms after boot/reset are we up again.
            if ((millis() - m_lastact) <= 20) {
                return;
            }
            if (m_ccs811->begin(CCS811_ADDRESS, m_wire)) {
                new_state = STATE_ACTIVE;
                dump_info();
                sample();
            } else {
                Serial << F("AirQualitySensorComponent: CCS811: ") <<  // (idefix)
                    F("communication failure\r\n");
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
    // FIXME: the Adafruit_CCS811 lib does not show these:
    // 17:06:20.786915: Hardware ID:           0x81
    // 17:06:20.786936: Hardware Version:      0x12
    // 17:06:20.786962: Firmware Boot Version: 0x1000
    // 17:06:20.786990: Firmware App Version:  0x2000
    //
    Serial << F("AirQualitySensorComponent: CCS811: ") << F("enabled\r\n");
#if 0
    Serial.print(F("Hardware ID:           0x"));
    Serial.println(m_ccs811->getHWID(), HEX);
    Serial.print(F("Hardware Version:      0x"));
    Serial.println(m_ccs811->getHWVersion(), HEX);
    Serial.print(F("Firmware Boot Version: 0x"));
    Serial.println(m_ccs811->getFWBootVersion(), HEX);
    Serial.print(F("Firmware App Version:  0x"));
    Serial.println(m_ccs811->getFWAppVersion(), HEX);
    Serial.println();
#endif
}

void AirQualitySensorComponent::sample()
{
    // FIXME: use SimpleKalmanFilter here (and for DHT11)
    uint16_t ccs_eco2;  // CCS811 eCO2
    uint16_t ccs_tvoc;  // CCS811 TVOC

    // Read the sensor data, this updates multiple fields
    // in the CCS811 library
    if (!m_ccs811->available()) {
        if (m_ccs811->checkError()) {
            Serial << F("ERROR: CCS811 ERROR flag set\r\n");
            // FIXME: print/show/decode errors..
            Device.set_alert(Device::INACTIVE_CCS811);
            m_state = STATE_FAILING;
        } else {
            Serial << F("CCS811: Data not ready\r\n");
        }
        return;
    }

    uint8_t error_id = m_ccs811->readData(); // 0x4 == MEASMODE_INVALID
    // WARNING: Do not call available() _after_ readData(). That would
    // cause the following sequence of messages:
    // 19:37:32.905514: I2CWRITE @ 0x5A :: 0x0,  <-- read status
    // 19:37:32.905591: I2CREAD  @ 0x5A :: 0x98,
    // 19:37:32.905616: I2CWRITE @ 0x5A :: 0x2,  <-- get data
    // 19:37:32.905642: I2CREAD  @ 0x5A :: 0x5, 0x91, 0x0, 0x9C, 0x98, 0x4, 0xD, 0x7E,
    // 19:37:32.905691: I2CWRITE @ 0x5A :: 0x0,  <-- read status
    // 19:37:32.905719: I2CREAD  @ 0x5A :: 0x90,
    // That available() returns 0x90, which means (READY_FOR_WORK, but no data):
    // - 0x01 = ERROR
    // - 0x06 = reserved
    // - 0x08 = NEW_DATA
    // - 0x10 = APP_VALID
    // - 0x60 = reserved
    // - 0x80 = READY_FOR_WORK
    (void)error_id; // ignore for now

    bool good_data = true;

    // Data is ready
    ccs_eco2 = m_ccs811->geteCO2();
    ccs_tvoc = m_ccs811->getTVOC();

    if (ccs_eco2 > 4000) {
        Serial << F("AirQualitySensorComponent: CCS811: ") <<  // (idefix)
            F("eCO2 exceeded limit\r\n");
        good_data = false;
    }
    if (ccs_tvoc > 1500) {
        Serial << F("AirQualitySensorComponent: CCS811: ") <<  // (idefix)
            F("TVOC exceeded limit\r\n");
        good_data = false;
    }

    Serial << F("AirQualitySensorComponent: ") <<  // (idefix)
        ccs_eco2 << F(" ppm(eCO2),  ") <<  // (idefix)
        ccs_tvoc << F(" ppb(TVOC), ");
    Serial.print(m_ccs811->getBaseline(), HEX);
    Serial << F(" opaque baseline\r\n");

    // Publish values
    if (good_data) {
        Device.clear_alert(Device::INACTIVE_CCS811);
        Device.publish("pe32/hud/co2/xwwwform", (
            String("eco2=") + ccs_eco2 + String("&tvoc=") + ccs_tvoc +
            String("&baseline=") + m_ccs811->getBaseline()));
    }
}
