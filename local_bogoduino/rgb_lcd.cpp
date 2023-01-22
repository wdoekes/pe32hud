#include <rgb_lcd.h>

rgb_lcd::rgb_lcd() {}

void rgb_lcd::begin(uint8_t cols, uint8_t rows, uint8_t charsize) {}
void rgb_lcd::clear() {}
void rgb_lcd::setCursor(uint8_t, uint8_t) {}
void rgb_lcd::setRGB(unsigned char r, unsigned char g, unsigned char b) {}

// Virtual
size_t rgb_lcd::write(uint8_t) { return 1; }
