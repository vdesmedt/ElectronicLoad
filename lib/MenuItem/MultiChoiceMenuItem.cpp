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

        if (this->_onChange(this->currentChoiceIndex))
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
        *focus = true;
        return false;
    }
}
bool MultiChoiceMenuItem::LongClick(bool *focus, uint8_t *page)
{
    *focus = !*focus;
    return false;
}

//TODO : review leading space logic
void MultiChoiceMenuItem::Print(LiquidCrystal_PCF8574 *lcd)
{
    char *cc = this->GetCurrentChoice();
    lcd->print(cc);
    for (uint8_t i = 0; i < this->GetChoiceMaxLength() - strlen(cc); i++)
    {
        lcd->print(" ");
    }
}