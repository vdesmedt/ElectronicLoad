#ifndef GotoPageMenuItem_h_
#define GotoPageMenuItem_h_

#include <Arduino.h>
#include <MenuItem.h>

class GoToPageMenuItem : public MenuItem
{
public:
    GoToPageMenuItem(const char *title, uint8_t destinationPageIndex, uint8_t cursorX, uint8_t cursorY) : MenuItem(GoToPage, cursorX, cursorY)
    {
        _title = title;
        targetPageIndex = destinationPageIndex;
    };

    void Print(LiquidCrystal_PCF8574 *lcd);
    bool Click(bool *focus, uint8_t *page);
    uint8_t targetPageIndex = -1;

protected:
    const char *_title = NULL;
};

#endif