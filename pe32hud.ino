// vim: set ts=8 sw=2 sts=2 et ai:
#include "config.h"

// TODO:
// - monitor ping to GW only (for wifi-health only)
// - the rest is fetched from remote
// - rename remote /watt/ to /hud/ or something

#include <Arduino.h>
#include <rgb_lcd.h>

/* Include files specific to the platform (ESP8266, Arduino or TEST) */
#if defined(ARDUINO_ARCH_ESP8266)
# define HAVE_ESP8266HTTPCLIENT
//# define HAVE_ESP8266PING
# define HAVE_ESP8266WIFI
# include <ESP8266WiFi.h>
# include <ESP8266HTTPClient.h>
#elif defined(ARDUINO_ARCH_AVR)
/* nothing yet */
#elif defined(TEST_BUILD)
//# define HAVE_ESP8266PING
/* nothing yet */
#endif

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

enum {
  COLOR_RED = 0xff0000,
  COLOR_YELLOW = 0xffff00,
  COLOR_GREEN = 0x00ff00,
  COLOR_BLUE = 0x0000ff
};

/**
 * HACKS: assume there's a WiFi object floating around.
 */
String whatsMyIntGateway() {
#ifdef HAVE_ESP8266WIFI
    return WiFi.gatewayIP().toString();
#else
    return "192.168.1.1";
#endif
}

static bool hudUpdate;
static String message0;
static String message1;
static long bgColor;
static String wattMessage;
static int wattUpdate;

void setup()
{
  Serial.begin(115200);

  lcd.begin(16, 2); /* 16 cols, 2 rows */

  lcd.clear();
  lcd.setColor(COLOR_YELLOW);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

#ifdef HAVE_ESP8266WIFI
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
     delay(1000);
  }
#endif
  delay(100);

  // Start
  bgColor = COLOR_YELLOW;
  wattMessage = String("ENOHTTP");
}

long watt2color(int watt) {
  if (watt <= -1024) {
    return COLOR_GREEN;
  } else if (watt <= -512) {
    long val = (-watt / 2) - 256;
    return 0x00ff00 + (0xff - val);
  } else if (watt < 0) {
    long val = (-watt / 2);
    return 0x0000ff + (val << 8);
  } else if (watt < 512) {
    long val = (watt / 2);
    return 0x0000ff + (val << 16);
  } else if (watt < 1024) {
    long val = (watt / 2) - 256;
    return 0xff00ff - val;
  } else {
    return COLOR_RED;
  }
}

static void fetchWatt()
{
#ifdef HAVE_ESP8266WIFI
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
#endif
#ifdef HAVE_ESP8266HTTPCLIENT
  HTTPClient http;
  http.begin(watt_url);
  int httpCode = http.GET();
  if (httpCode >= 200 && httpCode < 300) {
    String payload = http.getString();
    int watt = atoi(payload.c_str());
    bgColor = watt2color(watt);
    int lf = payload.indexOf('\n');
    if (lf >= 0) {
      message0 = payload.substring(lf + 1);
      lf = message0.indexOf('\n');
      if (lf >= 0) {
        message1 = message0.substring(lf + 1);
        message0.remove(lf);
      }
    }
    wattMessage = String(watt) + " W";
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
    fetchWatt();
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

  return 0;
}
#endif
