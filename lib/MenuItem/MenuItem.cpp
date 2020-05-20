#include <MenuItem.h>

MenuItem::MenuItem(uint8_t cursorX, uint8_t cursorY)
{
    cursor = 20 * cursorY + cursorX;
}