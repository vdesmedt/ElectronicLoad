#include <MenuItem.h>

MenuItem::MenuItem(enum menuItemType type, uint8_t cursorX, uint8_t cursorY)
{
    this->_menuType = type;
    this->cursorX = cursorX;
    this->cursorY = cursorY;
}