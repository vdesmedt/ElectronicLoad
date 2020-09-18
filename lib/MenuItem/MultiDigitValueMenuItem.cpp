#include <MultiDigitValueMenuItem.h>
#include <Arduino.h>

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
        DigitIndex = (DigitIndex + 1) % (_lenght - (AllowNeg ? 1 : 0) - (_precision > 0 ? 1 : 0));
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
    int8_t sp = _lenght - (AllowNeg ? 1 : 0) - (_precision > 0 ? 1 : 0) - DigitIndex - 1;
    int32_t newValue = _value + increment * ipow(10, sp);
    if (_onChange(newValue))
    {
        _value = newValue;
        return true;
    }
    return false;
}

const char *MultiDigitValueMenuItem::GetLabel()
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
    return buffer;
}
uint8_t MultiDigitValueMenuItem::GetCursorOffset(bool focus)
{
    if (focus)
    {
        uint8_t shift = DigitIndex;
        if (_precision > 0 && shift >= _lenght - _precision - 1)
            shift++;
        if (_prefix)
            shift += _prefixLenth;
        return shift;
    }
    else
    {
        if (_prefix)
            return _prefixLenth;
        return 0;
    }
}

enum MultiDigitValueMenuItem::cursorType MultiDigitValueMenuItem::GetCursorType(bool focus)
{
    return focus ? cursorType::Blink : cursorType::Normal;
}

uint8_t MultiDigitValueMenuItem::PrintLabel(U8G2 *lcd, uint8_t x, uint8_t y, bool current, bool hasFocus)
{
    const char *lbl = GetLabel();
    uint8_t pi = lcd->getColorIndex();

    if (current)
    {
        if (hasFocus)
        {
            uint8_t cu = GetCursorOffset(true) - _prefixLenth;
            char *buffer = " ";
            for (uint8_t i = 0; i < strlen(lbl); i++)
            {
                buffer[0] = lbl[i];
                if (i == cu && (millis() / 100) % 10 < 6)
                {
                    lcd->drawBox(
                        x - 1,
                        y - lcd->getMaxCharHeight() + 1,
                        lcd->getStrWidth(buffer) + 2,
                        lcd->getMaxCharHeight());
                    lcd->setColorIndex(0);
                    lcd->drawStr(x, y, buffer);
                    lcd->setColorIndex(pi);
                }
                else
                {
                    lcd->drawStr(x, y, buffer);
                }
                x += lcd->getStrWidth(buffer) + 1;
            }
            return x - 1;
        }
        else
        {
            lcd->drawBox(
                x - 1,
                y - lcd->getMaxCharHeight() + 1,
                lcd->getStrWidth(lbl) + 2,
                lcd->getMaxCharHeight());
            lcd->setColorIndex(0);
            lcd->drawStr(x, y, this->GetLabel());
            lcd->setColorIndex(pi);
            return x + lcd->getStrWidth(lbl);
        }
    }
    else
    {
        lcd->drawStr(x, y, lbl);
        return x + lcd->getStrWidth(lbl);
    }
}