// vim: set ts=8 sw=2 sts=2 et ai:
#include "config.h"

#include <Arduino.h>
#include <rgb_lcd.h>

/* Include files specific to the platform (ESP8266, Arduino or TEST) */
#if defined(ARDUINO_ARCH_ESP8266)
# define HAVE_ESP8266HTTPCLIENT
# define HAVE_ESP8266PING
# define HAVE_ESP8266WIFI
# include <ESP8266WiFi.h>
# include <ESP8266HTTPClient.h>
#elif defined(ARDUINO_ARCH_AVR)
/* nothing yet */
#elif defined(TEST_BUILD)
# define HAVE_ESP8266PING
/* nothing yet */
#endif

#ifdef HAVE_ESP8266PING
# include "PingMon.h"
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

#ifdef HAVE_ESP8266PING
PingMon pingMon;
#endif

enum {
  COLOR_RED = 0xff0000,
  COLOR_YELLOW = 0xffff00,
  COLOR_GREEN = 0x00ff00,
  COLOR_BLUE = 0x0000ff
};

/**
 * Return 111.222.33.44 if that's my external IP.
 */
String whatsMyIp() {
    static String ret;
    static long lastMs = 0;
    const long cacheFor = (15 * 60 * 1000); // 15 minutes

    if (lastMs == 0 || (millis() - lastMs) > cacheFor) {
#ifdef HAVE_HTTPCLIENT
        HTTPClient http;
        http.begin(whatsmyip_url);
        int httpCode = http.GET();
        if (httpCode >= 200 && httpCode < 300) {
            String body = http.getString();
            body.trim();
            if (body.length()) {
                ret = body;
            }
        }
        http.end();
#else
        ret = "111.222.33.44";
#endif
        lastMs = millis();
    }
    return ret;
}

/**
 * If whatsMyIp is 111.222.33.44, then whatsMyExtGateway is 111.222.33.1.
 */
String whatsMyExtGateway() {
    String ret = whatsMyIp();
    int pos = ret.lastIndexOf('.'); // take last '.'
    if (pos > 0) {
        ret.remove(pos + 1);
        ret += "1"; // append "1", assume the gateway is at "x.x.x.1"
    }
    return ret;
}

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
static long bgColor;
static String wattMessage;
static int wattUpdate;
static float worstPingLoss;
static unsigned worstPingMs;
static const char* worstPingHost;

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
  worstPingLoss = 0;
  worstPingMs = 0;
  worstPingHost = "no.host";

#ifdef HAVE_ESP8266PING
  pingMon.addTarget("dns.cldfl", "1.1.1.1");
  pingMon.addTarget("dns.ggle", "8.8.8.8");
  pingMon.addTarget("myip.ext", whatsMyIp);
  pingMon.addTarget("gw.ext", whatsMyExtGateway);
  pingMon.addTarget("gw.int", whatsMyIntGateway);
  pingMon.addTarget("host.cust", interesting_ip_custom);
#endif
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
    wattMessage = String(watt) + " W";
  } else {
    bgColor = COLOR_YELLOW;
    wattMessage = String("HTTP/") + httpCode;
  }
  http.end();
#endif
}

static void updateHud()
{
  lcd.setColor(bgColor);
  lcd.clear();
  if (worstPingHost) {
    lcd.setCursor(0, 0);
    lcd.print(worstPingMs);
    lcd.print("ms ");
    if (worstPingLoss > 0.0) {
      lcd.print((int)worstPingLoss);
      lcd.print("% ");
    }
    lcd.print(worstPingHost);
  }
  lcd.setCursor(0, 1);
  lcd.print(wattMessage.c_str());
}

void loop()
{
  pingMon.update();

  if ((millis() - wattUpdate) >= 15000) {
    fetchWatt();
    wattUpdate = millis();
    hudUpdate = true;
  }

  // Get worst ping/loss
  {
    float pingLoss = -1;
    unsigned pingMs = 0;
    const char* pingHost = NULL;
    for (unsigned i = 0; i < pingMon.getTargetCount(); ++i) {
      Target& tgt = pingMon.getTarget(i);
      const PingStats& st = tgt.getStats();
      if (st.loss > pingLoss) {
        pingLoss = st.loss;
        pingMs = st.responseTimeMs;
        pingHost = tgt.getId();
      } else if (st.loss == pingLoss && st.responseTimeMs > pingMs) {
        pingMs = st.responseTimeMs;
        pingHost = tgt.getId();
      }
    }
    if (pingLoss != worstPingLoss || pingMs != worstPingMs || pingHost != worstPingHost) {
      worstPingLoss = pingLoss;
      worstPingMs = pingMs;
      worstPingHost = pingHost;
      hudUpdate = true;
    }
  }

  if (hudUpdate) {
    updateHud();
    hudUpdate = false;
  }
}

#if TEST_BUILD
void test_pingmon()
{
    PingMon pingMon;
    pingMon.addTarget("dns.cloudflare", "1.1.1.1");
    pingMon.addTarget("dns.google", "8.8.8.8");
    pingMon.addTarget("myip.ext", whatsMyIp);
    pingMon.addTarget("gw.ext", whatsMyExtGateway);
    pingMon.addTarget("gw.int", whatsMyIntGateway);
    pingMon.addTarget("host.custom", interesting_ip_custom);
    for (unsigned i = 0; i < pingMon.getTargetCount(); ++i) {
        Target& tgt = pingMon.getTarget(i);
        printf("%s (%s)\n", tgt.getId(), tgt.getHost().c_str());
        tgt.addResponse(200, 12);
    }
    for (unsigned i = 0; i < pingMon.getTargetCount(); ++i) {
        Target& tgt = pingMon.getTarget(i);
        printf("%s (%s)\n", tgt.getId(), tgt.getHost().c_str());
        tgt.addResponse(300, 11);
        const PingStats st = tgt.getStats();
        printf("  stats: %f %u %u\n", st.loss, st.responseTimeMs, st.ttl);
    }
}
#endif

#if TEST_BUILD
#include "xtoa.h"
int main() {
  test_pingmon();

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
