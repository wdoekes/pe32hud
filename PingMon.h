#ifndef INCLUDED_PINGMON_H
#define INCLUDED_PINGMON_H

#include <Arduino.h>

/* Include files specific to the platform (ESP8266, Arduino or TEST) */
#if defined(ARDUINO_ARCH_ESP8266)
# include <ESP8266HTTPClient.h>
# define HAVE_HTTPCLIENT
# define HAVE_GLOBALPINGER
# define HAVE_GLOBALWIFI
# include <Pinger.h> /* library: ESP8266-ping */
extern Pinger pinger;
#elif defined(ARDUINO_ARCH_AVR)
/* nothing yet */
#elif defined(TEST_BUILD)
/* nothing yet */
#endif

//typedef String (*GetHostnameFunc)();
using GetHostnameFunc = String(void);


class PingStats {
public:
    PingStats(float loss, float responseTimeMs, float ttl) {
        this->loss = loss;
        this->responseTimeMs = responseTimeMs;
        this->ttl = ttl;
    }
    float loss;
    unsigned responseTimeMs;
    unsigned char ttl;
};


class Target {
private:
#ifdef HAVE_GLOBALPINGER
    static Target* _onPingResponseTarget;
    static inline bool _onPingResponse(const PingerResponse& response);
#endif

    const char *_name;
    mutable String _hostname; /* or IP */
    GetHostnameFunc *_getHostnameFunc;

    static const unsigned _history = 4;
    int _responseTimeMs[_history];
    unsigned char _ttls[_history];
    unsigned char _histPtr;

    long _lastResponseMs;
    unsigned _totalResponses;

    void _init() {
        for (unsigned i = 0; i < _history; ++i) {
            _responseTimeMs[i] = -32768; /* unset */
            _ttls[i] = 255;
        }
        _histPtr = 0;
        _lastResponseMs = 0;
        _totalResponses = 0;
    }

public:
    void reset(const char *name, const char* hostname) {
        _init();
        _name = name;
        _hostname.remove(0);
        _hostname += hostname;
        _getHostnameFunc = nullptr;
    }
    void reset(const char *name, String hostname) {
        _init();
        _name = name;
        _hostname = hostname;
        _getHostnameFunc = nullptr;
    }
    void reset(const char *name, GetHostnameFunc getHostnameFunc) {
        _init();
        _name = name;
        _getHostnameFunc = getHostnameFunc;
    }

    const char *getId() const {
        if (!_name) {
            return "<INVALID>";
        }
        return _name;
    }
    const String& getHost() const {
        /* TODO: don't call this every time please.. */
        if (_getHostnameFunc) {
            _hostname = _getHostnameFunc(); // mutable(!) _hostname
        }
        return _hostname;
    }

    void addResponse(int responseTimeMs, int ttl) {
        _responseTimeMs[(int)_histPtr] = responseTimeMs;
        _ttls[(int)_histPtr] = (
            (ttl >= 0 && ttl < 255) ? (unsigned char)ttl : 255);
        _histPtr = (_histPtr + 1) % _history;
    }
    void addResponseTimeout() {
        return addResponse(-1, -1);
    }

    const PingStats getStats() const;
    void update();
};


class PingMon {
private:
    static const unsigned char _maxTargets = 6;
    unsigned char _nTargets;
    unsigned char _curTarget;
    Target _dests[_maxTargets];

public:
    PingMon() : _nTargets(0), _curTarget(0) {}

    const unsigned getTargetCount() const { return _nTargets; }
    template<class T> void addTarget(const char *name, T hostnameOrFunc) {
        if (_nTargets >= _maxTargets)
            return;
        _dests[_nTargets++].reset(name, hostnameOrFunc);
    }
    Target& getTarget(int i) {
        return _dests[i];
    }

    /**
     * Do zero or more ping updates. Depending on the current time and
     * how much we have done already.
     */
    void update() {
        long t0 = millis();
        unsigned i;
        for (i = 0; i < _nTargets; ++i) {
            unsigned curTarget = (i + _curTarget) % _nTargets;
            _dests[curTarget].update();
            /* If we're doing more than 500ms, don't attempt to do
             * anything else. */
            if ((millis() - t0) > 500) {
                ++i; /* start at next */
                break;
            }
        }
        _curTarget = (i + _curTarget) % _nTargets;
    }
};

// vim: set ts=8 sw=4 sts=4 et ai:
#endif //INCLUDED_PINGMON_H
