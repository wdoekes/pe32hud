/**
 * pe32hud // LCD screen, displaying current status
 *
 * Components:
 * - ESP8266 (NodeMCU, with Wifi and this HTTP support).
 * - attach PIN_LCD_SCL<->SCL, PIN_LCD_SDA<->SDA, Vin<->VCC, GND<->GND
 *   (using Vin to get 5V directly from USB instead of 3V3 from the board);
 * - attach Somfy remote; XXX FIXME.
 *
 * Building/dependencies:
 * - Arduino IDE
 * - (for ESP8266) board: package_esp8266com_index.json
 *
 * Configuration:
 * - Connect the Grove-LCD RGB Backlight 4 pins according to specs.
 * - Connect the Somfy soldered remote; XXX FIXME;
 * - Copy arduino_secrets.h.example to arduino_secrets.h and fill in your
 *   Wifi credentials and HUD URL configuration.
 */
#include "pe32hud.h"

#ifdef HAVE_ESP8266WIRE
#include <Wire.h>
#endif
#include <rgb_lcd.h>

// The default pins are defined in variants/nodemcu/pins_arduino.h as
// SDA=4 and SCL=5. [...] You can also choose the pinds yourself using
// the I2C constructor Wire.begin(int sda, int scl);
// D1..D7 are all good, D0 is not.
// See: https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
static constexpr int PIN_LCD_SCL = 5;  // D1 / GPIO5
static constexpr int PIN_LCD_SDA = 4;  // D2 / GPIO4

static constexpr int LCD_ROWS = 2;
static constexpr int LCD_COLS = 16;

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
static String wattMessage;
static int wattUpdate;

static void parseHudData(String hudData)
{
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
    }
  }
}

void setup()
{
  Serial.begin(115200);

#ifdef HAVE_ESP8266WIRE
  Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL);
#endif

  lcd.begin(LCD_COLS, LCD_ROWS); /* 16 cols, 2 rows */

  lcd.clear();
  lcd.setColor(COLOR_YELLOW);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

#ifdef HAVE_ESP8266WIFI
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
     delay(1000);
  }
#endif
  delay(100);

  // Start
  bgColor = COLOR_YELLOW;
  wattMessage = String("ENOHTTP");
}

static void fetchHud()
{
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
    wattMessage = String("HTTP/") + httpCode;
    message0 = wattMessage;
    message1 = "(error)";
  }
  http.end();
#endif
}

static void updateHud()
{
  lcd.setColor(bgColor);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message0.c_str());
  lcd.setCursor(0, 1);
  lcd.print(message1.c_str());
}

void loop()
{
  if ((millis() - wattUpdate) >= 15000) {
    fetchHud();
    wattUpdate = millis();
    hudUpdate = true;
  }

  if (hudUpdate) {
    updateHud();
    hudUpdate = false;
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
