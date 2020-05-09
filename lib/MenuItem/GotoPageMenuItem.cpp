#include <GotoPageMenuItem.h>

void GoToPageMenuItem::Print(LiquidCrystal_PCF8574 *lcd)
{
    lcd->print(_title);
}

bool GoToPageMenuItem::Click(bool *focus, uint8_t *page)
{
    *focus = false;
    *page = this->targetPageIndex;
    return true;
}