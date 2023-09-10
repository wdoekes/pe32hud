#ifndef INCLUDED_PE32HUD_DISPLAYCOMPONENT_H
#define INCLUDED_PE32HUD_DISPLAYCOMPONENT_H

#include "pe32hud.h"

// Display on I2C, with a 16x2 matrix
static constexpr int LCD_ROWS = 2;
static constexpr int LCD_COLS = 16;

class rgb_lcd_plus;

class DisplayComponent {
private:
    rgb_lcd_plus* m_lcd;
    String m_message0;
    String m_message1;
    unsigned long m_bgcolor;
    bool m_hasupdate;

public:
    DisplayComponent(TwoWire* theWire = &Wire);

    void setup();
    void loop();

    void set_text(String msg0, String msg1, uint32_t color);

private:
    void show();
};

#endif //INCLUDED_PE32HUD_DISPLAYCOMPONENT_H
