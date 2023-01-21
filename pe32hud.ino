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

#ifdef HAVE_ESP8266WIRE
#include <Wire.h>
#endif
#include <rgb_lcd.h>

#include "CCS811.h"

// The default pins are defined in variants/nodemcu/pins_arduino.h as
// SDA=4 and SCL=5. [...] You can also choose the pinds yourself using
// the I2C constructor Wire.begin(int sda, int scl);
// D1..D7 are all good, D0 is not.
// See: https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
static constexpr int PIN_SCL = 5;  // D1 / GPIO5
static constexpr int PIN_SDA = 4;  // D2 / GPIO4

static constexpr int LCD_ROWS = 2;
static constexpr int LCD_COLS = 16;

// LEDs are shared with GPIO pins 0 and 2
static constexpr int LED_ON = LOW;
static constexpr int LED_OFF = HIGH;
static constexpr int LED_RED = 0;
static constexpr int LED_BLUE = 2;

static constexpr int CCS811_RST = 12;

// We use output HIGH for all these PINs so they're not detected as
// grounded. A button press would mean ground: which we simulate by
// setting output to LOW for a short while.
static constexpr int SOMFY_SEL = 14;  // D5 / GPIO14
static constexpr int SOMFY_DN = 13;   // D7 / GPIO13
static constexpr int SOMFY_UP = 15;   // D8 / GPIO15


class rgb_lcd_plus : public rgb_lcd {
public:
  void setColor(long color) {
    setRGB(
      (color & 0xff0000) >> 16,
      (color & 0x00ff00) >> 8,
      (color & 0x0000ff));
  }
};

rgb_lcd_plus lcd;

#ifdef HAVE_ESP8266WIFI
WiFiClient wifiClient;
#endif

enum {
  COLOR_RED = 0xff0000,
  COLOR_YELLOW = 0xffff00,
  COLOR_GREEN = 0x00ff00,
  COLOR_BLUE = 0x0000ff
};

static bool hudUpdate;
static String message0;
static String message1;
static long bgColor;
static long hudUpdateLast;

int action = 0;

static void parseHudData(String hudData) {
  int start = 0;
  bool done = false;

  bgColor = -1L;
  message0 = "";
  message1 = "";

  while (!done) {
    String line;
    int lf = hudData.indexOf('\n', start);
    if (lf < 0) {
      line = hudData.substring(start);
      done = true;
    } else {
      line = hudData.substring(start, lf);
      start = lf + 1;
    }

    if (line.startsWith("color:#")) {
      bgColor = strtol(line.c_str() + 7, NULL, 16);
    } else if (line.startsWith("line0:")) {
      message0 = line.substring(6, 6 + LCD_COLS);
    } else if (line.startsWith("line1:")) {
      message1 = line.substring(6, 6 + LCD_COLS);
    } else if (line.startsWith("action:UP")) {
      if (action <= 0) {
        action = 1;
      }
    } else if (line.startsWith("action:RESET")) {
      action = 0;
    } else if (line.startsWith("action:DOWN")) {
      if (action >= 0) {
        action = -1;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

#ifdef HAVE_ESP8266WIRE
  Wire.begin(PIN_SDA, PIN_SCL);
#endif

  lcd.begin(LCD_COLS, LCD_ROWS); /* 16 cols, 2 rows */

  lcd.clear();
  lcd.setColor(COLOR_YELLOW);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, LED_ON);
  digitalWrite(LED_RED, LED_ON);

  pinMode(CCS811_RST, OUTPUT);
  digitalWrite(CCS811_RST, HIGH);  // not reset

  pinMode(SOMFY_SEL, OUTPUT);
  pinMode(SOMFY_DN, OUTPUT);
  pinMode(SOMFY_UP, OUTPUT);
  digitalWrite(SOMFY_SEL, HIGH);
  digitalWrite(SOMFY_DN, HIGH);
  digitalWrite(SOMFY_UP, HIGH);

#ifdef HAVE_ESP8266WIFI
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
#endif
  delay(100);

  // Start
  bgColor = COLOR_YELLOW;
  digitalWrite(LED_BLUE, LED_OFF);
}

static void fetchHud() {
#ifdef HAVE_ESP8266WIFI
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
#endif
#ifdef HAVE_ESP8266HTTPCLIENT
  HTTPClient http;
  http.begin(wifiClient, SECRET_HUD_URL);
  int httpCode = http.GET();
  if (httpCode >= 200 && httpCode < 300) {
    // Fetch data and truncate just in case.
    String payload = http.getString().substring(0, 2048);
    parseHudData(payload);
  } else {
    bgColor = COLOR_YELLOW;
    message0 = String("HTTP/") + httpCode;
    message1 = "(error)";
  }
  http.end();
#endif
}

static void updateHud() {
  lcd.setColor(bgColor);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message0.c_str());
  lcd.setCursor(0, 1);
  lcd.print(message1.c_str());
  Serial << "HUD [" << message0 << "] [" << message1 << "]\r\n";
}

// CCS811 I2C Interface
CCS811 ccs;

bool hasCcs = false;

//*****************************************************************************
// Function: sampleCCS811
// Sample and update CCS811 data from sensor.
//*****************************************************************************
void sampleCCS811() {
  uint16_t ccs_eco2;  // CCS811 eCO2
  uint16_t ccs_tvoc;  // CCS811 TVOC
  uint8_t ccs_error;  // CCS811 error register

  //Serial.println(F("Reading CCS811 Sensor"));

  // Read the sensor data, this updates multiple fields
  // in the CCS811 library
  ccs.readAlgResultDataRegister();

  // Read error register if an error was reported
  if (ccs.hasERROR()) {
    Serial.println(F("ERROR: CCS811 Error Flag Set"));

    ccs_error = ccs.getERROR_ID();
    Serial.print(F("CCS811 Error Register = "));
    Serial.println(ccs_error);
    Serial.println();
    hasCcs = false;
    return;
  }

  // Data is ready
  if (ccs.isDATA_READY()) {
    ccs_eco2 = ccs.geteCO2();
    ccs_tvoc = ccs.getTVOC();

    // Verify eCO2 is valid
    if (ccs_eco2 > CCS811_ECO2_MAX) {
      Serial.print(F("(ERROR: CCS811 eCO2 Exceeded Limit)  "));
    }

    // Print eCO2 to serial monitor
    Serial.print(F("eCO2 = "));
    Serial.print(ccs_eco2);
    Serial.print(F(" ppm  "));

    // Verify TVOC is valid
    if (ccs_tvoc > CCS811_TVOC_MAX) {
      Serial.print(F("(ERROR: CCS811 TVOC Exceeded Limit)  "));
    }

    // Print TVOC to serial monitor
    Serial.print(F("TVOC = "));
    Serial.print(ccs_tvoc);
    Serial.print(F(" ppb"));
  } else {
    Serial.print(F("(ERROR: CCS811 Data Not Ready)  "));
    //hasCcs = false;
  }

  Serial.println();
}

void loop() {
  if ((millis() - hudUpdateLast) >= 5000) {
    fetchHud();
    hudUpdateLast = millis();
    hudUpdate = true;

    if (!hasCcs) {
      digitalWrite(LED_RED, LED_ON);
      // Force reset, we _must_ call begin() after this.
      digitalWrite(CCS811_RST, LOW);
      delay(20);  // reset/wake pulses must be at least 20us (micro!)
      digitalWrite(CCS811_RST, HIGH);
      delay(20);  // 20ms until after boot/reset are we up again
      if (ccs.begin(0x5A)) {
        hasCcs = true;
        // Print CCS811 sensor information
        Serial.println(F("CCS811 Sensor Enabled:"));
        Serial.print(F("Hardware ID:           0x"));
        Serial.println(ccs.getHWID(), HEX);
        Serial.print(F("Hardware Version:      0x"));
        Serial.println(ccs.getHWVersion(), HEX);
        Serial.print(F("Firmware Boot Version: 0x"));
        Serial.println(ccs.getFWBootVersion(), HEX);
        Serial.print(F("Firmware App Version:  0x"));
        Serial.println(ccs.getFWAppVersion(), HEX);
        Serial.println();
      } else {
        Serial.println(F("ERROR: No CCSCould not find a valid CCS811 sensor"));
      }
      digitalWrite(LED_RED, LED_OFF);
    }
  }

  if (hudUpdate) {
    if (action == 1) {
      lcd.setColor(COLOR_YELLOW);
      digitalWrite(SOMFY_UP, LOW);
      delay(600);
      digitalWrite(SOMFY_UP, HIGH);
      action = 2;
      lcd.setColor(bgColor);
    }
    if (action == -1) {
      lcd.setColor(COLOR_YELLOW);
      digitalWrite(SOMFY_DN, LOW);
      delay(600);
      digitalWrite(SOMFY_DN, HIGH);
      action = -2;
      lcd.setColor(bgColor);
    }
    updateHud();
    hudUpdate = false;

    if (hasCcs) {
      // Sample CCS811 Data
      sampleCCS811();
    }
  }
}

#if TEST_BUILD
#include "xtoa.h"
int main() {
  char buf[30];
  dtostrf(1234.5678, 15, 2, buf);
  printf("[%s]\n", buf);

  String s;
  s += "test123-";
  s += (double)1234.5678;
  printf("[%s]\n", s.c_str());

  String payload(
    "color:#00ff68\n"
    "line0: -814 W    39 msXXXXXX\n"
    "line1:^11.981  v 5.637");
  parseHudData(payload);
  printf("[color == 00ff68 == %06lx]\n", bgColor);
  printf("[line0 == %s]\n", message0.c_str());
  printf("[line1 == %s]\n", message1.c_str());

  return 0;
}
#endif

/* vim: set ts=8 sw=2 sts=2 et ai: */