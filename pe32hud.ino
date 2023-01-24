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

#include "CCS811.h"

#define DEBUG

// The default pins are defined in variants/nodemcu/pins_arduino.h as
// SDA=4 and SCL=5. [...] You can also choose the pins yourself using
// the I2C constructor Wire.begin(int sda, int scl);
// D1..D7 are all good, D0 is not.
// See: https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
static constexpr int PIN_SCL = 5;  // D1 / GPIO5
static constexpr int PIN_SDA = 4;  // D2 / GPIO4

// Display on I2C, with a 16x2 matrix
static constexpr int LCD_ROWS = 2;
static constexpr int LCD_COLS = 16;

// Air quality sensor on I2C, with a reset PIN (LOW to reset)
static constexpr int CCS811_RST = 12;

// LEDs are shared with GPIO pins 0 and 2
static constexpr int LED_ON = LOW;
static constexpr int LED_OFF = HIGH;
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
// UTILS
//

class rgb_lcd_plus : public rgb_lcd {
public:
  inline void setColor(long color) {
    setRGB(
      (color & 0xff0000) >> 16,
      (color & 0x00ff00) >> 8,
      (color & 0x0000ff));
  }
};


////////////////////////////////////////////////////////////////////////
// COMMON INTERFACE
//

class Device {
public:
  enum action {
    ACTION_SUNSCREEN = 0x7,
    ACTION_SUNSCREEN_NONE = 0x1,
    ACTION_SUNSCREEN_SELECT = 0x2,
    ACTION_SUNSCREEN_DOWN = 0x3,
    ACTION_SUNSCREEN_UP = 0x4
  };
  enum {
    COLOR_RED = 0xff0000,
    COLOR_YELLOW = 0xffff00,
    COLOR_GREEN = 0x00ff00,
    COLOR_BLUE = 0x0000ff
  };

private:
  enum action m_lastsunscreen;

public:
  Device()
    : m_lastsunscreen(ACTION_SUNSCREEN_NONE) {}
  void set_text(const String& msg0, const String& msg1, unsigned long color);
  void set_error(const String& msg0, const String& msg1);
  void add_action(enum action atn);
};

Device Device;


////////////////////////////////////////////////////////////////////////
// COMPONENTS
//

class AirQualitySensorComponent {
private:
  static constexpr unsigned long m_interval = 30000;
  unsigned long m_lastact;
  bool m_ready;
  CCS811 m_ccs811;

public:
  void setup() {
    pinMode(CCS811_RST, OUTPUT);
    digitalWrite(CCS811_RST, HIGH);  // not reset
  }

  void loop() {
    if ((millis() - m_lastact) >= m_interval) {
#ifdef DEBUG
      Serial << F("  --AirQualitySensorComponent: sample/reset\r\n");
#endif
      m_lastact = millis();
      if (m_ready) {
        sample();
      } else {
        reset();
      }
    }
  }

private:
  void reset() {
    // XXX: we don't want delay like this...
    digitalWrite(LED_RED, LED_ON);
    // Force reset, we _must_ call begin() after this.
    digitalWrite(CCS811_RST, LOW);
    delay(20);  // reset/wake pulses must be at least 20us (micro!)
    digitalWrite(CCS811_RST, HIGH);
    delay(20);               // 20ms until after boot/reset are we up again
    if (m_ccs811.begin()) {  // 0x5A
      m_ready = true;
      // Print CCS811 sensor information
      Serial.println(F("CCS811 Sensor Enabled:"));
      Serial.print(F("Hardware ID:           0x"));
      Serial.println(m_ccs811.getHWID(), HEX);
      Serial.print(F("Hardware Version:      0x"));
      Serial.println(m_ccs811.getHWVersion(), HEX);
      Serial.print(F("Firmware Boot Version: 0x"));
      Serial.println(m_ccs811.getFWBootVersion(), HEX);
      Serial.print(F("Firmware App Version:  0x"));
      Serial.println(m_ccs811.getFWAppVersion(), HEX);
      Serial.println();
    } else {
      Serial.println(F("ERROR: No CCSCould not find a valid CCS811 sensor"));
    }
    digitalWrite(LED_RED, LED_OFF);
  }

  void sample() {
    uint16_t ccs_eco2;  // CCS811 eCO2
    uint16_t ccs_tvoc;  // CCS811 TVOC
    uint8_t ccs_error;  // CCS811 error register

    //Serial.println(F("Reading CCS811 Sensor"));

    // Read the sensor data, this updates multiple fields
    // in the CCS811 library
    m_ccs811.readAlgResultDataRegister();

    // Read error register if an error was reported
    if (m_ccs811.hasERROR()) {
      Serial.println(F("ERROR: CCS811 Error Flag Set"));

      ccs_error = m_ccs811.getERROR_ID();
      Serial.print(F("CCS811 Error Register = "));
      Serial.println(ccs_error);
      Serial.println();
      m_ready = false;
      return;
    }

    // Data is ready
    if (m_ccs811.isDATA_READY()) {
      ccs_eco2 = m_ccs811.geteCO2();
      ccs_tvoc = m_ccs811.getTVOC();

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
      Serial << ccs_tvoc << F(" ppb(TVOC)");
    } else {
      Serial.print(F(" (ERROR: CCS811 Data Not Ready) "));
      //hasCcs = false;
    }

    Serial.println();
  }
};


class DisplayComponent {
private:
  rgb_lcd_plus m_lcd;
  String m_message0;
  String m_message1;
  unsigned long m_bgcolor;
  bool m_hasupdate;

public:
  DisplayComponent()
    : m_message0(F("Initializing...")),  // (fix ide wrap)
      m_bgcolor(Device::COLOR_YELLOW), m_hasupdate(true) {}

  void setup() {
    m_lcd.begin(LCD_COLS, LCD_ROWS);  // 16 cols, 2 rows
  }

  void loop() {
    if (m_hasupdate) {
      Serial << F("  --DisplayComponent: show\r\n");
      show();
      m_hasupdate = false;
    }
  }

  void set_text(String msg0, String msg1, unsigned long color) {
    m_message0 = msg0;
    m_message1 = msg1;
    m_bgcolor = color;
    m_hasupdate = true;
  }

private:
  void show() {
    m_lcd.setColor(m_bgcolor);
    m_lcd.clear();
    m_lcd.setCursor(0, 0);
    m_lcd.print(m_message0.c_str());
    m_lcd.setCursor(0, 1);
    m_lcd.print(m_message1.c_str());
    Serial << F("HUD:    [") <<  // header
      m_message0 << F("] [") <<  // top message
      m_message1 << F("]\r\n");  // bottom message
  }
};


class LedStatusComponent {
private:
  enum blink_mode {
    BLINK_WIFI
  };

public:
  void setup() {
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    // Blue led ON during boot (or errors). Red can show stuff whenever.
    digitalWrite(LED_BLUE, LED_ON);
    digitalWrite(LED_RED, LED_OFF);
  }

  void loop() {
    /* fixme: how do we know what to show? */
  }

#if 0
  void blink(uint8_t pin, enum blink_mode how, unsigned long wait) {
    unsigned long waited;
    if (how == BLINK_WIFI) {
      digitalWrite(pin, LED_ON);
      delay(350);
      digitalWrite(pin, LED_OFF);
      delay(100);
      digitalWrite(pin, LED_ON);
      delay(150);
      digitalWrite(pin, LED_OFF);
      waited = 600;
    } else {
      digitalWrite(pin, LED_ON);
      delay(100);
      digitalWrite(pin, LED_OFF);
      waited = 100;
    }
    if (wait >= waited) {
      wait -= waited;
    } else {
      wait = 0;
    }
    if (wait) {
      delay(wait);
    }
  }
#endif
};


class NetworkComponent {
#ifdef TEST_BUILD
  friend int main(int argc, char** argv);
#endif

public:
  struct RemoteResult {
    String message0;
    String message1;
    unsigned long color;
    enum Device::action sunscreen;
  };

private:
  static constexpr unsigned long m_interval = 5000;
  unsigned long m_lastact;
#ifdef HAVE_ESP8266WIFI
  WiFiClient m_wificlient;
  unsigned m_connectfails;
#endif

public:
  void setup() {
#ifdef HAVE_ESP8266WIFI
    WiFi.mode(WIFI_STA);
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
#endif
  }

  void loop() {
#ifdef HAVE_ESP8266WIFI
    ensure_wifi();
#endif
    if ((millis() - m_lastact) >= m_interval) {
      Serial << F("  --NetworkComponent: fetch/update\r\n");
      String remote_packet = fetch_remote();
      m_lastact = millis();  // after poll, so we don't hammer on failure

      if (remote_packet.length()) {
        RemoteResult res;
        parse_remote(remote_packet, res);
        handle_remote(res);
      }
    }
  }

private:
#ifdef HAVE_ESP8266WIFI
  void ensure_wifi() {
    wl_status_t wifi_status = WiFi.status();
    digitalWrite(LED_BLUE, ((wifi_status == WL_CONNECTED) ? LED_OFF : LED_ON));

    if (wifi_status == WL_CONNECTED) {
      return;
    }

    int8_t ret = WiFi.waitForConnectResult(15000);
    if (ret != -1) {
      wifi_status = static_cast<wl_status_t>(ret);
    }

    switch (wifi_status) {
      case WL_CONNECTED:
        digitalWrite(LED_BLUE, LED_OFF);
        m_connectfails = 0;
        break;
      case WL_IDLE_STATUS:
      case WL_NO_SSID_AVAIL:
      case WL_CONNECT_FAILED:
      case WL_DISCONNECTED:
        m_connectfails += 1;
        Device.set_error(String(F("Wifi state ")) + wifi_status, String(F("")) + m_connectfails + F(" attempts"));
        if (m_connectfails == 1) {
          Serial << F("\r\n");
          WiFi.printDiag(Serial);
          Serial << F("\r\n");
        }
        break;
#ifdef WL_CONNECT_WRONG_PASSWORD
      case WL_CONNECT_WRONG_PASSWORD:
        m_connectfails += 1;
        Device.set_error(F("Wifi wrong creds."), String(F("")) + m_connectfails + F(" attempts"));
        break;
#endif
      default:
        m_connectfails += 1;
        Device.set_error(String(F("Wifi unknown ")) + wifi_status, String(F("")) + m_connectfails + F(" attempts"));
        WiFi.printDiag(Serial);
        break;
    }
  }
#endif

  String fetch_remote() {
    String payload;
#ifdef HAVE_ESP8266HTTPCLIENT
    HTTPClient http;
    http.begin(m_wificlient, SECRET_HUD_URL);
    int http_code = http.GET();
    if (http_code >= 200 && http_code < 300) {
      // Fetch data and truncate just in case.
      payload = http.getString().substring(0, 512);
    } else {
      Device.set_error(String(F("HTTP/")) + http_code, F("(error)"));
    }
    http.end();
#endif
    return payload;
  }

  static void parse_remote(const String& remote_packet, RemoteResult& res) {
    int start = 0;
    bool done = false;

    res.color = Device::COLOR_YELLOW;

    while (!done) {
      String line;
      int lf = remote_packet.indexOf('\n', start);
      if (lf < 0) {
        line = remote_packet.substring(start);
        done = true;
      } else {
        line = remote_packet.substring(start, lf);
        start = lf + 1;
      }

      if (line.startsWith("color:#")) {
        res.color = strtol(line.c_str() + 7, NULL, 16);
      } else if (line.startsWith("line0:")) {
        res.message0 = line.substring(6, 6 + LCD_COLS);
      } else if (line.startsWith("line1:")) {
        res.message1 = line.substring(6, 6 + LCD_COLS);
      } else if (line.startsWith("action:UP")) {
        res.sunscreen = Device::ACTION_SUNSCREEN_UP;
      } else if (line.startsWith("action:RESET")) {
        res.sunscreen = Device::ACTION_SUNSCREEN_NONE;
      } else if (line.startsWith("action:DOWN")) {
        res.sunscreen = Device::ACTION_SUNSCREEN_DOWN;
      }
    }
  }

  void handle_remote(const RemoteResult& res) {
    Device.set_text(res.message0, res.message1, res.color);
    Device.add_action(res.sunscreen);
  }
};


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

public:
  void setup() {
    pinMode(SOMFY_SEL, OUTPUT);
    pinMode(SOMFY_DN, OUTPUT);
    pinMode(SOMFY_UP, OUTPUT);
    digitalWrite(SOMFY_SEL, HIGH);
    digitalWrite(SOMFY_DN, HIGH);
    digitalWrite(SOMFY_UP, HIGH);
  }

  void loop() {
    if (m_state == DEPRESSED) {
      ;
    } else if (m_state & REQUEST) {
      handle_press_request();
    } else if ((millis() - m_lastact) >= m_interval) {
      handle_depress();
    }
  }

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
  void press_at_most_one(enum state st) {
    digitalWrite(SOMFY_SEL, st == REQUEST_SEL ? LOW : HIGH);
    digitalWrite(SOMFY_DN, st == REQUEST_DN ? LOW : HIGH);
    digitalWrite(SOMFY_UP, st == REQUEST_UP ? LOW : HIGH);
  }

  void handle_press_request() {
    // TODO: something with flickering/blinking?
    // lcd.setColor(COLOR_YELLOW)?
#ifdef DEBUG
    Serial << F("  --SunscreenComponent: pressing ") << m_state << F("\r\n");
#endif
    press_at_most_one(m_state);
    m_state = static_cast<enum state>(m_state & ~REQUEST);
  }

  void handle_depress() {
#ifdef DEBUG
    Serial << F("  --SunscreenComponent: depressing ") << m_state << F("\r\n");
#endif
    press_at_most_one(DEPRESSED);
    m_state = DEPRESSED;
  }
};


class TemperatureSensorComponent {
private:
  static constexpr unsigned long m_interval = 30000;
  unsigned long m_lastact;
  DHTesp m_dht11;

public:
  void setup() {
    m_dht11.setup(PIN_DHT11, DHTesp::DHT11);
    m_lastact = (millis() - m_interval);
  }

  void loop() {
    if ((millis() - m_lastact) >= m_interval) {
#ifdef DEBUG
      Serial << F("  --TemperatureSensorComponent: sample\r\n");
#endif
      m_lastact = millis();
      sample();
    }
  }

private:
  void sample() {
    float humidity = m_dht11.getHumidity();
    float temperature = m_dht11.getTemperature();
    Serial << F("DHT11:  ") <<                         // (comment for Arduino IDE)
      m_dht11.getStatusString() << F(" status,  ") <<  // "OK"
      temperature << F(" 'C,  ") <<                    // (comment for Arduino IDE)
      humidity << F(" phi(RH)\r\n");                   // (comment for Arduino IDE)
  }
};


////////////////////////////////////////////////////////////////////////
// GLOBALS
//

AirQualitySensorComponent airQualitySensorComponent;
DisplayComponent displayComponent;
LedStatusComponent ledStatusComponent;
NetworkComponent networkComponent;
SunscreenComponent sunscreenComponent;
TemperatureSensorComponent temperatureSensorComponent;


////////////////////////////////////////////////////////////////////////
// DEVICE IMPLEMENTATION
// ... invoking tasks ...
//

void Device::set_text(const String& msg0, const String& msg1, unsigned long color) {
  displayComponent.set_text(msg0, msg1, color);
}

void Device::set_error(const String& msg0, const String& msg1) {
  displayComponent.set_text(msg0, msg1, COLOR_YELLOW);
}

void Device::add_action(enum action atn) {
  if (atn & ACTION_SUNSCREEN) {
    if (m_lastsunscreen != atn) {
      switch (atn) {
        case ACTION_SUNSCREEN_SELECT:
          sunscreenComponent.press_select();
          break;
        case ACTION_SUNSCREEN_DOWN:
          sunscreenComponent.press_down();
          break;
        case ACTION_SUNSCREEN_UP:
          sunscreenComponent.press_up();
          break;
        default:
          break;
      }
      m_lastsunscreen = atn;
    }
  }
}


////////////////////////////////////////////////////////////////////////
// main program
//

void setup() {
  delay(500);
  Serial.begin(115200);
  while (!Serial) {}

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
  for (int i = 0; i < 5; ++i) {
    printf("<<< loop %d >>>\n", i);
    loop();
    printf("\n");
  }

  return 0;
}
#endif

/* vim: set ts=8 sw=2 sts=2 et ai: */
