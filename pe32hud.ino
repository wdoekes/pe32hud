/**
 * pe32hud // LCD screen, displaying current status
 *
 * Components:
 * - ESP8266 (NodeMCU, with Wifi and this HTTP support).
 * - attach PIN_LCD_SCL<->SCL, PIN_LCD_SDA<->SDA, Vin<->VCC, GND<->GND
 *   (using Vin to get 5V directly from USB instead of 3V3 from the board);
 * - attach Somfy remote, pins SOMFY_DN, SOMFY_UP and 3V3 and GND. All
 *   available PINs must be detached or pulled up (output HIGH, or
 *   INPUT_PULLUP).
 *
 * Building/dependencies:
 * - Arduino IDE
 * - (for ESP8266) board: package_esp8266com_index.json
 *
 * Configuration:
 * - Connect the Grove-LCD RGB Backlight 4 pins according to specs.
 * - Connect the Somfy soldered remote: only using down and up right
 *   now. We need 3V3 and GND to be attached as well and we might just
 *   as well connect SOMFY_SEL while we're at it.
 * - Copy arduino_secrets.h.example to arduino_secrets.h and fill in your
 *   Wifi credentials and HUD URL configuration.
 */
#include "pe32hud.h"

#include "Device.h"

#include "AirQualitySensorComponent.h"
#include "DisplayComponent.h"
#include "LedStatusComponent.h"
#include "NetworkComponent.h"
#include "SunscreenComponent.h"
#include "TemperatureSensorComponent.h"

// The default pins are defined in variants/nodemcu/pins_arduino.h as
// SDA=4 and SCL=5. [...] You can also choose the pins yourself using
// the I2C constructor Wire.begin(int sda, int scl);
// D1..D7 are all good, D0 is not.
// See: https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
static constexpr int PIN_SCL = 5;  // D1 / GPIO5
static constexpr int PIN_SDA = 4;  // D2 / GPIO4

// Air quality sensor on I2C, with a reset PIN (LOW to reset)
static constexpr int CCS811_RST = 12;

// LEDs are shared with GPIO pins 0 and 2
static constexpr int LED_RED = 0;
static constexpr int LED_BLUE = 2;

// Temperature sensor using a single GPIO pin
static constexpr int PIN_DHT11 = 16;

// We use output HIGH for all these PINs so they're not detected as
// grounded. A button press would mean ground: which we simulate by
// setting output to LOW for a short while.
static constexpr int SOMFY_SEL = 14;  // D5 / GPIO14
static constexpr int SOMFY_DN = 13;   // D7 / GPIO13
static constexpr int SOMFY_UP = 15;   // D8 / GPIO15

#ifdef TEST_BUILD
int main(int argc, char** argv);
#endif

// esp/xtensa notes: Using F-strings below reduces RODATA (constants)
// and increases IROM (code in flash) usage. Both normal constants and
// static constexpr values are optimized away when possible.


////////////////////////////////////////////////////////////////////////
// INTERFACES (and implementations...?)
//
class StatusLed {
public:
  virtual void toggle(bool on) = 0;
};

class GpioLed : public StatusLed {
private:
  uint8_t const m_pin;
  int8_t const m_off;
  int8_t const m_on;
public:
  GpioLed(uint8_t pin, int8_t off, int8_t on)
    : m_pin(pin), m_off(off), m_on(on) {
    pinMode(m_pin, OUTPUT);
    digitalWrite(m_pin, m_off);
  }
  virtual void toggle(bool on) { digitalWrite(m_pin, on ? m_on : m_off); }
};

GpioLed redLed(LED_RED, LED_OFF, LED_ON);
GpioLed blueLed(LED_BLUE, LED_OFF, LED_ON);


////////////////////////////////////////////////////////////////////////
// GLOBALS
//

Device Device;  // the one and only Device

AirQualitySensorComponent airQualitySensorComponent(PIN_SDA, PIN_SCL, CCS811_RST);
DisplayComponent displayComponent(PIN_SDA, PIN_SCL);
LedStatusComponent ledStatusComponent(
  [](bool on){ redLed.toggle(on); }, [](bool on){ blueLed.toggle(on); });
// FIXME: pass passphrase here..
NetworkComponent networkComponent;
SunscreenComponent sunscreenComponent(SOMFY_SEL, SOMFY_DN, SOMFY_UP);
TemperatureSensorComponent temperatureSensorComponent(PIN_DHT11);


////////////////////////////////////////////////////////////////////////
// main program
//

void setup() {
  Device.set_displaycomponent(&displayComponent);
  Device.set_ledstatuscomponent(&ledStatusComponent);
  Device.set_networkcomponent(&networkComponent);
  Device.set_sunscreencomponent(&sunscreenComponent);

  delay(3000);
  Serial.begin(115200);
  while (!Serial) {}
  delay(500);
  Serial << F("Booting...\r\n");

  // FIXME: move to both??
  // The Wire needs to be configured for SDA/SCL. Both the
  // AirQualityComponent and the DisplayComponent interface through I2C.
#ifdef ARDUINO_ARCH_ESP8266
  Wire.begin(PIN_SDA, PIN_SCL);  // non-standard esp8266 invocation
#else
#ifdef ARDUINO_ARCH_ESP32
  Wire.setPins(PIN_SDA, PIN_SCL);
#endif
  Wire.begin();  // standard Arduino invocation
#endif

  airQualitySensorComponent.setup();
  displayComponent.setup();
  ledStatusComponent.setup();
  networkComponent.setup();
  sunscreenComponent.setup();
  temperatureSensorComponent.setup();
}


void loop() {
  airQualitySensorComponent.loop();
  displayComponent.loop();
  ledStatusComponent.loop();
  networkComponent.loop();
  sunscreenComponent.loop();
  temperatureSensorComponent.loop();
}


#if TEST_BUILD
#include "xtoa.h"
int main(int argc, char** argv) {
  char buf[30];
  dtostrf(1234.5678, 15, 2, buf);
  printf("[%s]\n", buf);

  String s;
  s += "test123-";
  s += (double)1234.5678;
  printf("[%s]\n", s.c_str());

  Serial.print("hex test (should be 0xA) 0x");
  Serial.print(10, HEX);
  Serial.println();

  String payload(
    "color:#00ff68\n"
    "line0: -814 W    39 msXXXXXX\n"
    "line1:^11.981  v 5.637\n"
    "action:UP");
  NetworkComponent::RemoteResult res;
  NetworkComponent::parse_remote(payload, res);
  printf("[color == 00ff68 == %06lx]\n", res.color);
  printf("[line0 == %s]\n", res.message0.c_str());
  printf("[line1 == %s]\n", res.message1.c_str());

  Serial.println("millis (3x):");
  Serial.println(millis());
  Serial.println(millis());
  Serial.println(millis());

  // Test setup and loop once
  printf("<<< setup >>>\n");
  setup();
  printf("\n");
  int i;
  unsigned long ms, lastms = millis();
  for (i = 0, ms = millis(); i < 50; ++i, ms += 105) {
    millis(ms);  // HACKS: set the milliseconds
    printf("<<< loop %d (%lu->%lu) >>>\n", i, lastms, ms);
    loop();
    printf("\n");
    lastms = millis();
  }

  return 0;
}
#endif

/* vim: set ts=8 sw=2 sts=2 et ai: */
