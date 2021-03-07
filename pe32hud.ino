#include "config.h"

#include <Arduino.h>
#include <rgb_lcd.h>

/* Include files specific to the platform (ESP8266, Arduino or TEST) */
#if defined(ARDUINO_ARCH_ESP8266)
# define HAVE_ESP8266
#elif defined(ARDUINO_ARCH_AVR)
/* nothing yet */
#elif defined(TEST_BUILD)
/* nothing yet */
#endif

#ifdef HAVE_ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif

rgb_lcd lcd;

const int colorR = 255;
const int colorG = 255;
const int colorB = 0;

enum {
  COLOR_RED = 0xff0000,
  COLOR_YELLOW = 0xffff00,
  COLOR_GREEN = 0x00ff00,
  COLOR_BLUE = 0x0000ff
};

long color = COLOR_YELLOW;

void lcdSetColor(rgb_lcd lcd, long color)
{
  lcd.setRGB(
    (color & 0xff0000) >> 16,
    (color & 0x00ff00) >> 8,
    (color & 0x0000ff));
}

void setup()
{
  lcd.begin(16, 2); /* 16 cols, 2 rows */

  lcd.clear();
  lcdSetColor(lcd, COLOR_YELLOW);
  lcd.setCursor(0, 0);
  lcd.print("Initiializing...");

#ifdef HAVE_ESP8266
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
     delay(1000);
  }
#endif
  delay(100);
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

void loop()
{
#ifdef HAVE_ESP8266
  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;

    http.begin(watt_url);
    int httpCode = http.GET();

    if (httpCode >= 200 && httpCode < 300) {
      String payload = http.getString();
      int val = atoi(payload.c_str());

      lcd.clear();
      lcdSetColor(lcd, color);
      lcd.setCursor(0, 0);
      lcd.print("Instant. power");
      lcd.setCursor(0, 1);
      color = watt2color(val);
      lcd.print(String(val) + " W " + String(color, HEX) + " ");
    } else {
      lcd.clear();
      lcdSetColor(lcd, COLOR_YELLOW);
      lcd.setCursor(0, 0);
      lcd.print("HTTP Error ");
    }

    http.end();

  } else {
    lcd.clear();
    lcdSetColor(lcd, COLOR_YELLOW);
    lcd.setCursor(0, 0);
    lcd.print("Wifi Error ");
  }
#endif

  delay(15000);
}

#if TEST_BUILD
int main() { return 0; }
#endif
