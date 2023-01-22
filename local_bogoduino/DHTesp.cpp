#include <Arduino.h>
#include <DHTesp.h>

void DHTesp::setup(unsigned char, DHTesp::DHT_MODEL_t) {
}

float DHTesp::getHumidity() {
    return 42.2;
}

float DHTesp::getTemperature() {
    return 17.5;
}

const char *DHTesp::getStatusString() {
    return "OK";
}
