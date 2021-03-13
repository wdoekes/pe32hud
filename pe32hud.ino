// vim: set ts=8 sw=2 sts=2 et ai:
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

void setup()
{
  Serial.begin(115200);

  lcd.begin(16, 2); /* 16 cols, 2 rows */

  lcd.clear();
  lcd.setColor(COLOR_YELLOW);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

#ifdef HAVE_ESP8266
  WiFi.begin(wifi_ssid, wifi_password);
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

      long color = watt2color(val);

      lcd.setColor(color);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Instant. power");
      lcd.setCursor(0, 1);
      lcd.print(String(val) + " W " + String(color, HEX) + " ");
    } else {
      lcd.setColor(COLOR_YELLOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("HTTP Error ");
      lcd.print(httpCode);
    }

    http.end();

  } else {
    lcd.setColor(COLOR_YELLOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wifi Error ");
  }
#endif

  delay(15000);
}

#if TEST_BUILD
int main() { return 0; }
#endif
