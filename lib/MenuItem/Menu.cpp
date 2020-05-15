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
void Menu::Configure(LiquidCrystal_PCF8574 *lcd, void (*onPageChange)(uint8_t, uint8_t))
{
    this->_onPageChange = onPageChange;
    this->_pageFirstIndexes[_currentPage] = _currentItem;
    this->_currentPage = 0;
    this->_currentItem = 0;
    this->_selected = false;
    this->_lcd = lcd;
    this->_onPageChange(this->_currentPage, 0);
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
        //! This assumes menu items are sorted along Y axis in the array
        _currentItem += steps;
        if (_currentItem < _pageFirstIndexes[_currentPage])
            _currentItem = _pageFirstIndexes[_currentPage];
        else if (_currentItem >= _pageFirstIndexes[_currentPage + 1])
            _currentItem = _pageFirstIndexes[_currentPage + 1] - 1;

        uint8_t nsl = _scrollLevel;
        if (steps > 0 && _menuItems[_currentItem]->cursorY - _scrollLevel > 3)
            nsl = _scrollLevel + 1;
        else if (_menuItems[_currentItem]->cursorY - _scrollLevel < 0)
            nsl = _scrollLevel - 1;

        if (nsl != _scrollLevel)
        {
            Serial.print("New Scroll Level:");
            Serial.println(_scrollLevel);
            _scrollLevel = nsl;
            _lcd->clear();
            this->Print();
        }
        this->PrintCursor();
    }
}
void Menu::Click()
{
    MenuItem *mi = _menuItems[this->_currentItem];
    bool focus = _selected;
    uint8_t page = _currentPage;
    bool refresh = mi->Click(&focus, &page);
    _selected = focus;

    if (page != _currentPage)
    {
        _currentPage = page;
        _scrollLevel = 0;
        _currentItem = _pageFirstIndexes[_currentPage];
        this->_onPageChange(_currentPage, _scrollLevel);
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
    MenuItem *mi;
    for (int i = this->_pageFirstIndexes[this->_currentPage]; i < this->_pageFirstIndexes[this->_currentPage + 1]; i++)
    {
        mi = _menuItems[i];
        if (mi->cursorY >= _scrollLevel && mi->cursorY - _scrollLevel < 4)
            this->PrintItem(this->_menuItems[i]);
    }
    this->PrintCursor();
}

void Menu::PrintItem(MenuItem *mi)
{
    _lcd->setCursor(mi->cursorX, mi->cursorY - _scrollLevel);
    _lcd->print(mi->GetPrefix());
    _lcd->print(mi->GetLabel());
    _lcd->print(mi->GetSufix());
}

void Menu::PrintCursor()
{
    MenuItem *mi = this->_menuItems[this->_currentItem];
    _lcd->setCursor(mi->cursorX + mi->GetCursorOffset(_selected), mi->cursorY - _scrollLevel);
    if (mi->GetCursorType(_selected) == MenuItem::cursorType::Normal)
    {
        _lcd->noBlink();
        _lcd->cursor();
    }
    else
    {
        _lcd->noCursor();
        _lcd->blink();
    }
}