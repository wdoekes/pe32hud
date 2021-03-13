#include "PingMon.h"

#ifdef HAVE_GLOBALPINGER
# include <Pinger.h> /* library: ESP8266-ping */
Pinger pinger;
#endif

#ifdef HAVE_GLOBALPINGER
Target* Target::_onPingResponseTarget;
#endif

const PingStats Target::getStats() const
{
    float loss = 100;
    unsigned sent = 0;
    unsigned lost = 0;
    unsigned responseTimeMs = 0;
    unsigned ttl = 0;
    for (unsigned i = 0; i < _history; ++i) {
        if (_responseTimeMs[i] >= 0) {
            sent++;
            responseTimeMs += _responseTimeMs[i];
            ttl += _ttls[i];
        } else if (_responseTimeMs[i] == -1) {
            sent++;
            lost++;
        }
    }
    if (sent > lost) {
        loss = (float)lost * 100.0 / (float)sent;
        responseTimeMs = responseTimeMs / (sent - lost);
        ttl = ttl / (sent - lost);
    }
    if (lost == sent) {
        responseTimeMs = 999; /* all is gone */
    }
    return PingStats(loss, responseTimeMs, ttl);
}

void Target::update()
{
    unsigned timePassed = (millis() - _lastResponseMs) / 1000;
    if (timePassed < 1 ||
            ((_totalResponses % _history) == (_history - 1) &&
             timePassed < 600)) { /* every 10 minutes */
        return;
    }

#ifdef HAVE_GLOBALPINGER
    /* Yuck. This is not the greates code. But the Pinger class
     * leaves me little room to do this properly. */
    _onPingResponseTarget = this;
    pinger.OnReceive(_onPingResponse);
    if (pinger.Ping(getHost(), 1 /* requests */, 1000 /* ms timeout */)) {
        /* Make it synchronous... */
        while (_onPingResponseTarget) {
            delay(10);
        }
    } else {
        Serial.print("ERROR: Something went wrong with ping to ");
        Serial.print(getId());
        Serial.print(", ");
        Serial.println(getHost());
    }
#endif
    _lastResponseMs = millis();
    _totalResponses += 1;
}

#ifdef HAVE_GLOBALPINGER
bool Target::_onPingResponse(const PingerResponse& response) {
    Target& tgt = *_onPingResponseTarget;
    // extern "C" {
    // # include <lwip/icmp.h> /* needed for icmp packet definitions */
    // }
    // response.DestIPAddress.toString().c_str(),
    // response.EchoMessageSize - sizeof(struct icmp_echo_hdr),
    if (response.ReceivedResponse) {
        tgt.addResponse(response.ResponseTime, response.TimeToLive);
    } else {
        tgt.addResponseTimeout();
    }
    _onPingResponseTarget = NULL; /* done */
    return false; /* (don't continue, but we only scheduled one anyway) */
}
#endif
