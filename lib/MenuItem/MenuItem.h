#ifndef MenuItem_h_
#define MenuItem_h_

#include <Arduino.h>
#include <LiquidCrystal_PCF8574.h>

class MenuItem
{
public:
    enum menuItemType
    {
        MultiChoice,
        MultiDigitValue,
        GoToPage,
    };

    MenuItem(enum menuItemType type, uint8_t cursorX, uint8_t cursorY);

    enum menuItemType GetType() { return this->_menuType; }
    virtual void Print(LiquidCrystal_PCF8574 *lcd) = 0;
    virtual void PrintCursor(LiquidCrystal_PCF8574 *lcd, bool focus);

    // returns true if Print is needed
    virtual bool RotaryIncrement(int8_t steps) { return true; };
    virtual bool Click(bool *focus, uint8_t *page) { return false; };
    virtual bool LongClick(bool *focus, uint8_t *page) { return false; };

    uint8_t cursorX = 0;
    uint8_t cursorY = 0;

protected:
    enum menuItemType _menuType;
};

#endif