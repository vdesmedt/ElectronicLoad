#include <Menu.h>
#include <avr/pgmspace.h>

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
void Menu::Configure(U8G2 *lcd, void (*onPageChange)(uint8_t, uint8_t))
{
    this->_onPageChange = onPageChange;
    this->_pageFirstIndexes[_currentPage] = _currentItem;
    this->_currentPage = 0;
    this->_currentItem = 0;
    this->_selected = false;
    this->_lcd = lcd;
    this->_onPageChange(this->_currentPage, 0);
}

void Menu::EncoderInc(int8_t steps)
{
    _lastInteraction = millis();
    if (_iddle)
    {
        _iddle = false;
        return;
    }
    if (this->_selected)
    {
        MenuItem *mi = this->_menuItems[this->_currentItem];
        mi->RotaryIncrement(steps);
    }
    else
    {
        //! This assumes menu items are sorted along Y axis in the array
        _currentItem += steps;
        //First check we are not out of page's boundaries
        if (_currentItem < _pageFirstIndexes[_currentPage])
            _currentItem = _pageFirstIndexes[_currentPage];
        else if (_currentItem >= _pageFirstIndexes[_currentPage + 1])
            _currentItem = _pageFirstIndexes[_currentPage + 1] - 1;

        //Then make sure its pointing to a visible menu item
        if (!_menuItems[_currentItem]->IsShown())
        {
            EncoderInc(steps / abs(steps));
            return;
        }

        //Scroll down
        if (steps > 0 && _menuItems[_currentItem]->getCy() - _scrollLevel > _lcd->getHeight())
            _scrollLevel = _menuItems[_currentItem]->getCy() - _lcd->getHeight() + 1;
        //Scroll Up
        if (steps < 0 && _menuItems[_currentItem]->getCy() < _scrollLevel + _lcd->getMaxCharHeight())
            _scrollLevel = _menuItems[_currentItem]->getCy() - _lcd->getMaxCharHeight();
    }
}
void Menu::Click()
{
    _lastInteraction = millis();
    if (_iddle)
    {
        _iddle = false;
        return;
    }
    MenuItem *mi = _menuItems[this->_currentItem];
    bool focus = _selected;
    uint8_t page = _currentPage;
    mi->Click(&focus, &page);
    _selected = focus;

    if (page != _currentPage)
    {
        _currentPage = page;
        _scrollLevel = 0;
        _currentItem = _pageFirstIndexes[_currentPage];
        this->_onPageChange(_currentPage, _scrollLevel);
    }
}
void Menu::LongClick()
{
    _lastInteraction = millis();
    if (_iddle)
    {
        _iddle = false;
        return;
    }
    MenuItem *mi = this->_menuItems[this->_currentItem];
    mi->LongClick(&_selected, &_currentPage);
}

void Menu::Print()
{
    if (_lastInteraction + _focusTimeoutSec * 1000 < millis())
    {
        _iddle = true;
    }
    MenuItem *mi;
    for (int i = this->_pageFirstIndexes[this->_currentPage]; i < this->_pageFirstIndexes[this->_currentPage + 1]; i++)
    {
        mi = _menuItems[i];
        if (mi->getCy() >= _scrollLevel && mi->getCy() - _scrollLevel <= _lcd->getHeight())
        {
            uint8_t y = mi->getCy() - _scrollLevel;
            if (mi->IsShown())
            {
                mi->Print(_lcd, y, !_iddle && mi == _menuItems[_currentItem], !_iddle && _selected);
            }
        }
    }
}