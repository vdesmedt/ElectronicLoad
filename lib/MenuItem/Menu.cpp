#include <Menu.h>

//TODO Remove necessity to specify item/page count by inspecting AddPage/AddMenuItem
Menu::Menu(uint8_t pageCount, uint8_t itemCount)
{
    this->_menuItems = (MenuItem **)malloc(sizeof(MenuItem *) * itemCount);
    this->_pageFirstIndexes = (uint8_t *)malloc(sizeof(uint8_t) * (pageCount + 1));
    this->_currentPage = 0;
    this->_currentItem = 0;
}
MultiChoiceMenuItem *Menu::AddMultiChoice(const char *choices[], uint8_t choiceCount, uint8_t cursorX, uint8_t cursorY, bool (*onChange)(int8_t), bool switchOnClick)
{
    MultiChoiceMenuItem *mi = new MultiChoiceMenuItem(choices, choiceCount, cursorX, cursorY, onChange);
    mi->switchOnClick = switchOnClick;
    this->_menuItems[this->_currentItem++] = mi;
    return mi;
}
MultiDigitValueMenuItem *Menu::AddValue(int32_t initialValue, uint8_t length, uint8_t precision, uint8_t cursorX, uint8_t cursorY, bool (*onChange)(int32_t))
{
    MultiDigitValueMenuItem *mi = new MultiDigitValueMenuItem(initialValue, length, precision, cursorX, cursorY, onChange);
    this->_menuItems[this->_currentItem++] = mi;
    return mi;
}
GoToPageMenuItem *Menu::AddGoToPage(uint8_t pageIndex, const char *title, uint8_t cursorX, uint8_t cursorY)
{
    GoToPageMenuItem *mi = new GoToPageMenuItem(title, pageIndex, cursorX, cursorY);
    this->_menuItems[this->_currentItem++] = mi;
    return mi;
}
void Menu::AddPage()
{
    this->_pageFirstIndexes[_currentPage] = this->_currentItem;
    this->_currentPage++;
}
void Menu::Configure(LiquidCrystal_PCF8574 *lcd, void (*onPageChange)(uint8_t))
{
    this->_onPageChange = onPageChange;
    this->_pageFirstIndexes[_currentPage] = _currentItem;
    this->_currentPage = 0;
    this->_currentItem = 0;
    this->_selected = false;
    this->_lcd = lcd;
    this->_onPageChange(this->_currentPage);
    this->Print();
    this->PrintCursor();
}

void Menu::EncoderInc(int8_t steps)
{
    if (this->_selected)
    {
        MenuItem *mi = this->_menuItems[this->_currentItem];
        if (mi->RotaryIncrement(steps))
        {
            this->PrintItem(mi);
            this->PrintCursor();
        }
    }
    else
    {
        this->_currentItem += steps;
        if (this->_currentItem < this->_pageFirstIndexes[this->_currentPage])
            this->_currentItem = this->_pageFirstIndexes[this->_currentPage];
        else if (this->_currentItem >= this->_pageFirstIndexes[this->_currentPage + 1])
            this->_currentItem = this->_pageFirstIndexes[this->_currentPage + 1] - 1;
        this->PrintCursor();
    }
}
void Menu::Click()
{
    MenuItem *mi = this->_menuItems[this->_currentItem];
    bool focus = _selected;
    uint8_t page = _currentPage;
    bool refresh = mi->Click(&focus, &page);
    _selected = focus;

    if (page != _currentPage)
    {
        this->_currentPage = page;
        this->_currentItem = this->_pageFirstIndexes[this->_currentPage];
        this->_onPageChange(this->_currentPage);
    }

    if (refresh)
        this->Print();
    else
        this->PrintCursor();
}
void Menu::LongClick()
{
    MenuItem *mi = this->_menuItems[this->_currentItem];
    mi->LongClick(&_selected, &_currentPage);
    this->PrintCursor();
}

void Menu::Print()
{
    for (int i = this->_pageFirstIndexes[this->_currentPage]; i < this->_pageFirstIndexes[this->_currentPage + 1]; i++)
    {
        this->PrintItem(this->_menuItems[i]);
    }
    this->PrintCursor();
}

void Menu::PrintItem(MenuItem *mi)
{
    this->_lcd->setCursor(mi->cursorX, mi->cursorY);
    mi->Print(_lcd);
}

void Menu::PrintCursor()
{
    MenuItem *mi = this->_menuItems[this->_currentItem];
    mi->PrintCursor(_lcd, _selected);
}