#include <MultiChoiceMenuItem.h>

bool MultiChoiceMenuItem::RotaryIncrement(int8_t steps)
{
    if (this->_choices)
    {
        static int8_t ni;
        ni = currentChoiceIndex + steps;
        if (ni < 0)
            ni = 0;
        else if (ni >= _choiceCount)
            ni = switchOnClick ? 0 : _choiceCount - 1;

        if (this->_onChange(ni))
        {
            this->currentChoiceIndex = ni;
            return true;
        }
    }
    return false;
}

bool MultiChoiceMenuItem::Click(bool *focus, uint8_t *page)
{
    if (switchOnClick)
    {
        RotaryIncrement(1);
        return true;
    }
    else
    {
        *focus = !*focus;
        return false;
    }
}
bool MultiChoiceMenuItem::LongClick(bool *focus, uint8_t *page)
{
    *focus = !*focus;
    return false;
}

const char *MultiChoiceMenuItem::GetLabel()
{
    return _choices[currentChoiceIndex];
}
uint8_t MultiChoiceMenuItem::GetCursorOffset(bool focus)
{
    if (_prefix)
        return _prefixLenth;
    return 0;
}
