#include <MenuItem.h>

MenuItem::MenuItem(uint8_t cursorX, uint8_t cursorY)
{
    cursor = 20 * cursorY + cursorX;
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
    _updateMask |= UM_PREFIX;
}
void MenuItem::SetSuffix(const char *suffix)
{
    _suffix = suffix;
    _updateMask |= UM_SUFFIX;
}
