#ifndef GotoPageMenuItem_h_
#define GotoPageMenuItem_h_

#include <Arduino.h>
#include <MenuItem.h>

class GoToPageMenuItem : public MenuItem
{
public:
    GoToPageMenuItem(const char *title, uint8_t destinationPageIndex, uint8_t cursorX, uint8_t cursorY) : MenuItem(cursorX, cursorY)
    {
        _title = title;
        targetPageIndex = destinationPageIndex;
    };

    virtual const char *GetLabel();
    virtual uint8_t GetCursorOffset(bool focus);

    bool Click(bool *focus, uint8_t *page);
    uint8_t targetPageIndex = -1;

protected:
    const char *_title = NULL;
};

#endif