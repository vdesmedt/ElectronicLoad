#include <MenuItem.h>

MenuItem::MenuItem(enum menuItemType type, uint8_t cursorX, uint8_t cursorY)
{
    this->_menuType = type;
    this->cursorX = cursorX;
    this->cursorY = cursorY;
}

void MenuItem::PrintCursor(LiquidCrystal_PCF8574 *lcd, bool focus)
{
    lcd->setCursor(cursorX, cursorY);
    if (focus)
    {
        lcd->blink();
        lcd->noCursor();
    }
    else
    {
        lcd->noBlink();
        lcd->cursor();
    }
}
