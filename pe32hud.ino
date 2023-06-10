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
  enum alert {
    BOOTING = 1,
    INACTIVE_WIFI = 2,
    INACTIVE_DHT11 = 4,
    INACTIVE_CCS811 = 8,
    NOTIFY_SUNSCREEN = 16
  };

private:
  /* We use the guid to store something unique to identify the device by.
   * For now, we'll populate it with the ESP8266 Wifi MAC address. */
  char m_guid[24]; // "EUI48:11:22:33:44:55:66"

  enum action m_lastsunscreen;
  uint8_t m_alerts;

public:
  Device()
    : m_lastsunscreen(ACTION_SUNSCREEN_NONE) { memcpy(m_guid, "EUI48:11:22:33:44:55:66", 24); }

  const char* get_guid() { return m_guid; }
  void set_guid(const String& guid) { strncpy(m_guid, guid.c_str(), sizeof(m_guid) - 1); }

  void set_text(const String& msg0, const String& msg1, unsigned long color);
  void set_error(const String& msg0, const String& msg1);

  void set_alert(enum alert al) { set_or_clear_alert(al, true); }
  void clear_alert(enum alert al) { set_or_clear_alert(al, false); }

  void add_action(enum action atn);

  void publish(const String& topic, const String& formdata);

private:
  void set_or_clear_alert(enum alert al, bool is_alert);
};

Device Device;


////////////////////////////////////////////////////////////////////////
// COMPONENTS
//

class AirQualitySensorComponent {
private:
  static constexpr unsigned long m_interval = 30000;  // 30s
  unsigned long m_lastact;
  enum state {
    STATE_NONE,
    STATE_RESETTING,
    STATE_WAKING,
    STATE_ACTIVE,
    STATE_FAILING
  } m_state;
  CCS811 m_ccs811;

public:
  void setup() {
    Device.set_alert(Device::INACTIVE_CCS811);
    pinMode(CCS811_RST, OUTPUT);
    digitalWrite(CCS811_RST, HIGH);  // not reset
  }

  void loop() {
    enum state new_state = STATE_NONE;

    switch (m_state) {
    case STATE_NONE:
      // Force reset, we _must_ call begin() after this.
      digitalWrite(CCS811_RST, LOW);
      new_state = STATE_RESETTING;
      break;
    case STATE_RESETTING:
      // Reset/wake pulses must be at least 20us, so 1ms is enough.
      if ((millis() - m_lastact) <= 1) {
        return;
      }
      digitalWrite(CCS811_RST, HIGH);
      new_state = STATE_WAKING;
      break;
    case STATE_WAKING:
      // At 20ms after boot/reset are we up again.
      if ((millis() - m_lastact) <= 20) {
        return;
      }
      if (m_ccs811.begin()) {  // addr 90/0x5A
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

private:
  void dump_info() {
    // Print CCS811 sensor information
    // FIXME: improved Serial << usage
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
  }

  void sample() {
    // FIXME: use SimpleKalmanFilter here (and for DHT11)
    uint16_t ccs_eco2;  // CCS811 eCO2
    uint16_t ccs_tvoc;  // CCS811 TVOC
    uint8_t ccs_error;  // CCS811 error register

    // Read the sensor data, this updates multiple fields
    // in the CCS811 library
    m_ccs811.readAlgResultDataRegister();

    // Read error register if an error was reported
    if (m_ccs811.hasERROR()) {
      // FIXME: improved Serial << usage
      Serial.println(F("ERROR: CCS811 Error Flag Set"));

      ccs_error = m_ccs811.getERROR_ID();
      Serial.print(F("CCS811 Error Register = "));
      Serial.println(ccs_error);
      Serial.println();
      Device.set_alert(Device::INACTIVE_CCS811);
      m_state = STATE_FAILING;
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
      Serial << ccs_tvoc << F(" ppb(TVOC)\n");

      // Publish values
      Device.publish("pe32/hud/co2/xwwwform", (
        String("eco2=") + ccs_eco2 + String("&tvoc=") + ccs_tvoc));
    } else {
      Serial.print(F(" (ERROR: CCS811 Data Not Ready)\n"));
    }
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
    Device.set_alert(Device::BOOTING);  // useless if set/clear in setup()
    m_lcd.begin(LCD_COLS, LCD_ROWS);  // 16 cols, 2 rows
    Device.clear_alert(Device::BOOTING);
  }

  void loop() {
    if (m_hasupdate) {
#ifdef DEBUG
      Serial << F("  --DisplayComponent: show\r\n");
#endif
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

public:
  void setup() {
    // Blue led ON during boot (or errors). Red can show stuff whenever.
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
  }

  void loop() {
    // Not doing anything?
    if (m_blinktime == NULL) {
      if (m_blinkmode != NO_BLINK) {
        // Start blinking.
        m_blinktime = m_blinktimes[m_blinkmode];
        //printf("lastact %lu (+%lu) val %hhd S\n", m_lastact, (millis() - m_lastact), *m_blinktime);
        digitalWrite(LED_BLUE, m_blinkmode == BLINK_NORMAL ? LED_OFF : LED_ON);
        digitalWrite(LED_RED, *m_blinktime > 0 ? LED_ON : LED_OFF);
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
        digitalWrite(LED_RED, *m_blinktime > 0 ? LED_ON : LED_OFF);
        m_lastact = millis();
      }
    // The current value is 0 and we've waited for a second.
    } else if ((millis() - m_lastact) >= 1000) {
      if (m_blinkmode != NO_BLINK) {
        // Restart blinking.
        m_blinktime = m_blinktimes[m_blinkmode];
        //printf("lastact %lu (+%lu) val %hhd R\n", m_lastact, (millis() - m_lastact), *m_blinktime);
        digitalWrite(LED_RED, *m_blinktime > 0 ? LED_ON : LED_OFF);
      } else {
        // Stop blinking.
        m_blinktime = NULL;
        digitalWrite(LED_BLUE, LED_OFF);
        digitalWrite(LED_RED, LED_OFF);
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
  unsigned long m_wifidowntime;
#ifdef HAVE_ESP8266WIFI
  wl_status_t m_wifistatus;
  // NOTE: We need a WiFiClient for _each_ component that does network
  // connections (httpclient and mqttclient), otherwise using one will
  // disconnect the TCP session of the other.
  // See also: https://forum.arduino.cc/t/simultaneous-mqtt-and-http-post/430361/7
  WiFiClient m_httpbackend;
  WiFiClient m_mqttbackend;
  MqttClient m_mqttclient;
#endif

public:
  NetworkComponent()
#ifdef HAVE_ESP8266WIFI
    : m_wifistatus(WL_DISCONNECTED), m_mqttclient(m_mqttbackend)
#endif
    {};

  void setup() {
    Device.set_guid(String("EUI48:") + WiFi.macAddress());
    Device.set_alert(Device::INACTIVE_WIFI);
    m_wifidowntime = millis();
#ifdef HAVE_ESP8266WIFI
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);  // FIXME: needed?
    handle_wifi_state_change(WL_IDLE_STATUS);
    m_wifistatus = WL_IDLE_STATUS;
    m_wifidowntime = m_lastact = millis();
    // Do not forget setId(). Some MQTT daemons will reject id-less connections.
    m_mqttclient.setId(String(Device.get_guid()).substring(0, 23));
#endif
  }

  void loop() {
#ifdef HAVE_ESP8266WIFI
    wl_status_t wifistatus;
    if ((millis() - m_lastact) >= 500 && (
        (wifistatus = WiFi.status()) != WL_CONNECTED || wifistatus != m_wifistatus)) {
      if (m_wifistatus != wifistatus) {
        handle_wifi_state_change(wifistatus);
        m_wifistatus = wifistatus;
        if (wifistatus == WL_CONNECTED) {
          ensure_mqtt();
          sample();
        }
      } else if (m_wifistatus != WL_CONNECTED && (millis() - m_wifidowntime) > 5000) {
        wifistatus = WL_IDLE_STATUS;
        handle_wifi_state_change(wifistatus);
        m_wifistatus = wifistatus;
        m_wifidowntime = millis();
      }
      m_lastact = millis();
    }
#endif
    if (m_wifistatus == WL_CONNECTED && (millis() - m_lastact) >= m_interval) {
      ensure_mqtt();
      sample();
      m_lastact = millis();  // after poll, so we don't hammer on failure
    }
  }

  void push_remote(String topic, String formdata) {
    Serial << F("push_remote: ") << topic << " :: " << "device_id=" << Device.get_guid() << "&" << formdata << "\n";
#if 1
    if (m_mqttclient.connected()) {
    m_mqttclient.beginMessage(topic);
    m_mqttclient.print("device_id=");
    m_mqttclient.print(Device.get_guid());
    m_mqttclient.print("&");
    m_mqttclient.print(formdata);
    m_mqttclient.endMessage();
    }
#endif
  }

private:
#ifdef HAVE_ESP8266WIFI
  void handle_wifi_state_change(wl_status_t wifistatus) {
    Serial << F("NetworkComponent: Wifi state ") << m_wifistatus << F(" -> ") << wifistatus << F("\r\n");

    if (m_wifistatus == WL_CONNECTED) {
      m_wifidowntime = millis();
    }
    String downtime(String("") + ((millis() - m_wifidowntime) / 1000) + " downtime");

    switch (wifistatus) {
      case WL_IDLE_STATUS:
        Device.set_alert(Device::INACTIVE_WIFI);
        Device.set_error(F("Wifi connecting"), downtime);
        WiFi.disconnect(true, true);
        WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
        Serial << F("NetworkComponent: Wifi connecting...\r\n");
        break;
      case WL_CONNECTED:
        Device.clear_alert(Device::INACTIVE_WIFI);
        digitalWrite(LED_BLUE, LED_OFF);
        break;
      case WL_NO_SSID_AVAIL:
      case WL_CONNECT_FAILED:
      case WL_DISCONNECTED:
        Device.set_alert(Device::INACTIVE_WIFI);
        Device.set_error(String(F("Wifi state ")) + wifistatus, downtime);
        Serial << F("--\r\n");
        WiFi.printDiag(Serial);  // FIXME/XXX: beware, shows password on serial output
        Serial << F("--\r\n");
        break;
#ifdef WL_CONNECT_WRONG_PASSWORD
      case WL_CONNECT_WRONG_PASSWORD:
        Device.set_alert(Device::INACTIVE_WIFI);
        Device.set_error(F("Wifi wrong creds."), downtime);
        break;
#endif
      default:
        Device.set_alert(Device::INACTIVE_WIFI);
        Device.set_error(String(F("Wifi unknown ")) + wifistatus, downtime);
        Serial << F("--\r\n");
        WiFi.printDiag(Serial);  // FIXME/XXX: beware, shows password on serial output
        Serial << F("--\r\n");
        break;
    }
  }
#endif

  void ensure_mqtt() {
    m_mqttclient.poll();
    if (!m_mqttclient.connected()) {
      if (m_mqttclient.connect(SECRET_MQTT_BROKER, SECRET_MQTT_PORT)) {
        Serial << F("NetworkComponent: MQTT connected to " SECRET_MQTT_BROKER "\r\n");
      } else {
        Serial << F("NetworkComponent MQTT connection to " SECRET_MQTT_BROKER " failed: ")
          << m_mqttclient.connectError() << F("\r\n");
      }
    }
  }

  void sample() {
#ifdef DEBUG
    Serial << F("  --NetworkComponent: fetch/update\r\n");
#endif
    String remote_packet = fetch_remote();
    if (remote_packet.length()) {
      RemoteResult res;
      parse_remote(remote_packet, res);
      handle_remote(res);
    }
  }

  String fetch_remote() {
    String payload;
#ifdef HAVE_ESP8266HTTPCLIENT
    HTTPClient http;
    http.begin(m_httpbackend, SECRET_HUD_URL);
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
  SunscreenComponent() {
    // Run this _before_ setup time. Otherwise we might press buttons before setup is called.
    // However, we're pushing buttons while flashing the device, unfortunately.
    pinMode(SOMFY_SEL, OUTPUT);
    pinMode(SOMFY_DN, OUTPUT);
    pinMode(SOMFY_UP, OUTPUT);
    digitalWrite(SOMFY_SEL, HIGH);
    digitalWrite(SOMFY_DN, HIGH);
    digitalWrite(SOMFY_UP, HIGH);
  }

  void setup() {
    Device.clear_alert(Device::NOTIFY_SUNSCREEN);
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
    m_lastact = millis();
  }

  void handle_press_request() {
    Device.set_alert(Device::NOTIFY_SUNSCREEN);
    // TODO: something with flickering/blinking?
    // lcd.setColor(COLOR_YELLOW)?
#ifdef DEBUG
    Serial << F("  --SunscreenComponent: pressing ") << m_state << F("\r\n");
#endif
    press_at_most_one(m_state);
    m_state = static_cast<enum state>(m_state & ~REQUEST);
  }

  void handle_depress() {
    Device.clear_alert(Device::NOTIFY_SUNSCREEN);
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
  // FIXME: use SimpleKalmanFilter here (and eco2)
  DHTesp m_dht11;

public:
  void setup() {
    Device.set_alert(Device::INACTIVE_DHT11);
    m_dht11.setup(PIN_DHT11, DHTesp::DHT11);
    m_lastact = (millis() - m_interval);
    Device.clear_alert(Device::INACTIVE_DHT11);
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

    // Print values
    Serial << F("DHT11:  ") <<                         // (comment for Arduino IDE)
      m_dht11.getStatusString() << F(" status,  ") <<  // "OK"
      temperature << F(" 'C,  ") <<                    // (comment for Arduino IDE)
      humidity << F(" phi(RH)\r\n");                   // (comment for Arduino IDE)

    // Publish values
    Device.publish("pe32/hud/temp/xwwwform", (
      String("status=") + m_dht11.getStatusString() +
      String("&temperature=") + temperature +
      String("&humidity=") + humidity));
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

void Device::set_or_clear_alert(enum alert al, bool is_alert) {
  if (is_alert) {
    m_alerts |= al;
  } else {
    m_alerts &= ~al;
  }
  if (m_alerts & NOTIFY_SUNSCREEN) {
    ledStatusComponent.set_blink(LedStatusComponent::BLINK_SUNSCREEN);
  } else if (m_alerts & INACTIVE_WIFI) {
    ledStatusComponent.set_blink(LedStatusComponent::BLINK_WIFI);
  } else if (m_alerts & INACTIVE_DHT11) {
    ledStatusComponent.set_blink(LedStatusComponent::BLINK_DHT11);
  } else if (m_alerts & INACTIVE_CCS811) {
    ledStatusComponent.set_blink(LedStatusComponent::BLINK_CCS811);
  } else if (m_alerts) {
    // What problems?
    ledStatusComponent.set_blink(LedStatusComponent::BLINK_BOOT);
  } else {
    ledStatusComponent.set_blink(LedStatusComponent::BLINK_NORMAL);
  }
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

void Device::publish(const String& topic, const String& formdata) {
    networkComponent.push_remote(topic, formdata);
}


////////////////////////////////////////////////////////////////////////
// main program
//

void setup() {
  delay(3000);
  Serial.begin(115200);
  while (!Serial) {}
  delay(500);
  Serial << F("Booting...\r\n");

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
