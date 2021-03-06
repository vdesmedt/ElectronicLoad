#ifndef MultiDigitMenuItem_h_
#define MultiDigitMenuItem_h_

#include <Arduino.h>
#include <MenuItem.h>

class MultiDigitValueMenuItem : public MenuItem
{
public:
    MultiDigitValueMenuItem(int32_t initialValue, uint8_t length, uint8_t precision, uint8_t cursorX, uint8_t cursorY, bool (*onChange)(int32_t) = NULL) : MenuItem(cursorX, cursorY)
    {
        this->_value = initialValue;
        this->_lenght = length;
        this->_precision = precision;
        this->_onChange = onChange;
    };

    bool RotaryIncrement(int8_t increment);
    bool Click(bool *focus, uint8_t *page);
    bool LongClick(bool *focus, uint8_t *page);

    virtual const char *GetLabel();
    virtual uint8_t GetCursorOffset(bool focus);
    virtual enum cursorType GetCursorType(bool focus);
    void SetValue(int32_t value)
    {
        this->_value = value;
    }
    int32_t GetValue() { return this->_value; }

    uint8_t GetPrecision() { return this->_precision; }
    void SetPrecision(uint8_t precision) { _precision = precision; }
    uint8_t GetLength() { return this->_lenght; }
    void SetLength(uint8_t length) { this->_lenght = length; }
    uint8_t PrintLabel(U8G2 *lcd, uint8_t x, uint8_t y, bool current, bool hasFocus);
    bool AllowNeg = false;
    uint8_t DigitIndex = 0;

    bool (*_onChange)(int32_t);

protected:
    int32_t _value = 0;
    uint8_t _lenght = 5;
    uint8_t _precision = 3;
};

#endif