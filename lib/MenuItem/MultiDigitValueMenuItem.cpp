#include <MultiDigitValueMenuItem.h>

int ipow(int base, int exp)
{
    int result = 1;
    for (;;)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return result;
}

bool MultiDigitValueMenuItem::Click(bool *focus, uint8_t *page)
{
    if (*focus)
    {
        DigitIndex = (DigitIndex + 1) % (_lenght - (AllowNeg ? 2 : 1));
    }
    else
    {
        *focus = true;
    }
    return false;
}
bool MultiDigitValueMenuItem::LongClick(bool *focus, uint8_t *page)
{
    *focus = false;
    return false;
}

bool MultiDigitValueMenuItem::RotaryIncrement(int8_t increment)
{
    int8_t sp = _lenght - (AllowNeg ? 2 : 1) - DigitIndex - 1;
    int32_t newValue = _value + increment * ipow(10, sp);
    if (_onChange(newValue))
    {
        _value = newValue;
        return true;
    }
    return false;
}

void MultiDigitValueMenuItem::Print(LiquidCrystal_PCF8574 *lcd)
{
    static char *buffer = (char *)malloc(_lenght + 1); // + 1 for trailing 0
    uint32_t v = _value;
    int8_t lastIndex = _lenght - 1;
    for (int8_t i = lastIndex; i >= 0; i--)
    {
        if (_precision > 0 && i == lastIndex - _precision)
        {
            buffer[i] = '.';
        }
        else if (AllowNeg && v < 0 && i == 0)
        {
            buffer[i] = '-';
        }
        else
        {
            buffer[i] = 48 + v % 10;
            v = v / 10;
        }
    }
    buffer[lastIndex + 1] = 0;
    lcd->print(buffer);
}

void MultiDigitValueMenuItem::PrintCursor(LiquidCrystal_PCF8574 *lcd, bool focus)
{
    if (focus)
    {
        uint8_t shift = DigitIndex;
        if (shift >= _lenght - _precision - 1)
            shift++;
        lcd->setCursor(cursorX + shift, cursorY);
        lcd->noCursor();
        lcd->blink();
    }
    else
    {
        lcd->setCursor(cursorX, cursorY);
        lcd->cursor();
        lcd->noBlink();
    }
}