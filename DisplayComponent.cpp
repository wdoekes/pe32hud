#include "DisplayComponent.h"

#include "Device.h"

#include <rgb_lcd.h>        // Grove_-_LCD_RGB_Backlight

extern Device Device;

class rgb_lcd_plus : public rgb_lcd {
public:
  inline void setColor(long color) {
    setRGB(
      (color & 0xff0000) >> 16,
      (color & 0x00ff00) >> 8,
      (color & 0x0000ff));
  }
};

DisplayComponent::DisplayComponent(TwoWire* theWire) :
    // FIXME: rbg_lcd.h does not actually use this theWire
    m_lcd(new rgb_lcd_plus),
    m_message0(F("Initializing...")),
    m_bgcolor(Device::COLOR_YELLOW),
    m_hasupdate(true)
{
}

void DisplayComponent::setup()
{
    Device.set_alert(Device::BOOTING);  // useless if set/clear in setup()
    m_lcd->begin(LCD_COLS, LCD_ROWS);  // 16 cols, 2 rows
    Device.clear_alert(Device::BOOTING);
}

void DisplayComponent::loop()
{
    if (m_hasupdate) {
#ifdef DEBUG
        Serial << F("  --DisplayComponent: show\r\n");
#endif
        show();
        m_hasupdate = false;
    }
}

void DisplayComponent::set_text(String msg0, String msg1, unsigned long color)
{
    m_message0 = msg0;
    m_message1 = msg1;
    m_bgcolor = color;
    m_hasupdate = true;
}

void DisplayComponent::show()
{
    m_lcd->setColor(m_bgcolor);
    m_lcd->clear();
    m_lcd->setCursor(0, 0);
    m_lcd->print(m_message0.c_str());
    m_lcd->setCursor(0, 1);
    m_lcd->print(m_message1.c_str());
    Serial << F("HUD:    [") <<  // header
        m_message0 << F("] [") <<  // top message
        m_message1 << F("]\r\n");  // bottom message
}
