#ifndef INCLUDED_PE32HUD_H
#define INCLUDED_PE32HUD_H

#include <Arduino.h>	// Serial, pinMode, INPUT, OUTPUT, ...
#include <Wire.h>	// Wire

/* Include files specific to the platform (ESP8266, Arduino or TEST) */
#if defined(ARDUINO_ARCH_ESP8266)
#define HAVE_ESP8266HTTPCLIENT
#define HAVE_ESP8266WIFI
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#elif defined(ARDUINO_ARCH_AVR)
/* nothing yet */
#elif defined(TEST_BUILD)
#define HAVE_ESP8266WIFI
#include <ESP8266WiFi.h>
#endif

/* */
#include <rgb_lcd.h>	// Grove_-_LCD_RGB_Backlight
#include <DHTesp.h>	// DHT_sensor_library_for_ESPx

#include "arduino_secrets.h"

/* Neat trick to let us do multiple Serial.print() using the << operator:
 * Serial << x << " " << y << LF; */
template<class T> inline Print &operator<<(Print &obj, T arg) {
  obj.print(arg);
  return obj;
};

#endif  //INCLUDED_PE32HUD_H
