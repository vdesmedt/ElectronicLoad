#ifndef Menu_h_
#define Menu_h_

#include <MultiChoiceMenuItem.h>
#include <MultiDigitValueMenuItem.h>
#include <GotoPageMenuItem.h>

class Menu
{
public:
    Menu(uint8_t pageCount, uint8_t itemCount);
    MultiChoiceMenuItem *AddMultiChoice(const char *multiChoices[], uint8_t choiceCount, uint8_t cursorX, uint8_t cursorY, bool (*onChange)(int8_t), bool switchOnClick = false);
    MultiDigitValueMenuItem *AddValue(int32_t initialValue, uint8_t length, uint8_t precision, uint8_t cursorX, uint8_t cursorY, bool (*onChange)(int32_t));
    GoToPageMenuItem *AddGoToPage(uint8_t pageIndex, const char *title, uint8_t cursorX, uint8_t cursorY);
    void Configure(LiquidCrystal_PCF8574 *lcd, void (*onPageChange)(uint8_t, uint8_t));
    void AddPage();
    void EncoderInc(int8_t steps);
    void Click();
    void LongClick();
    uint8_t GetCurrentPage() { return this->_currentPage; }
    void Print();
    void PrintItem(MenuItem *mi);
    void PrintCursor();

private:
    LiquidCrystal_PCF8574 *_lcd;
    MenuItem **_menuItems;
    uint8_t *_pageFirstIndexes;
    uint8_t _currentPage = 0;
    uint8_t _scrollLevel = 0;
    uint8_t _currentItem = 0;
    bool _selected = false;
    MenuItem *_focus = NULL;
    void (*_onPageChange)(uint8_t, uint8_t);
    void (*_onScroll)(uint8_t);
};
#endif