#ifndef INCLUDED_BOGO_PRINT_H
#define INCLUDED_BOGO_PRINT_H

#include "Arduino.h"

class Print {
public:
    virtual ~Print() {}
    size_t write(const uint8_t *buffer, size_t size) {
	return 1;
    }
    size_t write(const char *buffer, size_t size) {
      return write((const uint8_t *)buffer, size);
    }
    size_t print(char const *p) {
	return 1;
    }
};

#endif //INCLUDED_BOGO_PRINT_H
