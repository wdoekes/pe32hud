#ifndef LIB_ADAFRUIT_CCS811_H
#define LIB_ADAFRUIT_CCS811_H

#define CCS811_ADDRESS 0x5A

class Adafruit_CCS811 {
public:
  bool begin(uint8_t addr = CCS811_ADDRESS, TwoWire *theWire = &Wire) { return true; };
  bool available() { return true; }
  bool checkError() { return false; }
  uint8_t readData() { return 0x04; }
  uint16_t getTVOC() { return 1; }
  uint16_t geteCO2() { return 407; }
  // void setEnvironmentalData(float humidity, float temperature) {};
  uint16_t getBaseline() { return 0x3412; }
};

#endif
