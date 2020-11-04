#include <MenuItem.h>

MenuItem::MenuItem(uint8_t cursorX, uint8_t cursorY)
{
    _cursorX = cursorX;
    _cursorY = cursorY;
}

size_t getLength(const __FlashStringHelper *ifsh)
{
    PGM_P p = reinterpret_cast<PGM_P>(ifsh);
    size_t n = 0;
    while (1)
    {
        unsigned char c = pgm_read_byte(p++);
        if (c == 0)
            break;
        n++;
    }
    return n;
}
void MenuItem::SetPrefix(const __FlashStringHelper *prefix)
{
    _prefix = prefix;
    _prefixLenth = getLength(prefix);
}
void MenuItem::SetSuffix(const char *suffix)
{
    _suffix = suffix;
}

void MenuItem::Show()
{
    _state |= IS_SHOW;
}

bool MenuItem::IsShown()
{
    return _state & IS_SHOW;
}

void MenuItem::Print(U8G2 *lcd, uint8_t y, bool current, bool hasFocus)
{
    char buffer[20];
    uint8_t x = _cursorX;
    strcpy_P(buffer, (char *)this->GetPrefix());
    lcd->drawStr(x, y, buffer);
    x += lcd->getStrWidth(buffer) + 1;
    x = PrintLabel(lcd, x, y, current, hasFocus) + 1;
    lcd->drawStr(x, y, this->GetSufix());
}

uint8_t MenuItem::PrintLabel(U8G2 *lcd, uint8_t x, uint8_t y, bool current, bool hasFocus)
{
    if (current)
    {
        uint8_t pi = lcd->getColorIndex();
        if (!hasFocus || (millis() / 100) % 10 < 6)
        {
            lcd->drawBox(
                x - 1,
                y - lcd->getMaxCharHeight() + 1,
                lcd->getStrWidth(this->GetLabel()) + 2,
                lcd->getMaxCharHeight());
            lcd->setColorIndex(0);
        }
        lcd->drawStr(x, y, this->GetLabel());
        lcd->setColorIndex(pi);
    }
    else
    {
        lcd->drawStr(x, y, this->GetLabel());
    }
    return x + lcd->getStrWidth(this->GetLabel()) + 1;
}
