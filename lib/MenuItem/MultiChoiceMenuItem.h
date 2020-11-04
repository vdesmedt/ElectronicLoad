#ifndef MultiChoiceMenuItem_h_
#define MultiChoiceMenuItem_h_

#include <Arduino.h>
#include <MenuItem.h>

class MultiChoiceMenuItem : public MenuItem
{
public:
    MultiChoiceMenuItem(const char *choices[], uint8_t choicesCount, uint8_t cursorX, uint8_t cursorY, bool (*onChange)(int8_t) = NULL) : MenuItem(cursorX, cursorY)
    {
        _choices = (char **)choices;
        _choiceCount = choicesCount;
        _onChange = onChange;
        for (uint8_t i = 0; i < choicesCount; i++)
            this->_choiceMaxLength = max(this->_choiceMaxLength, strlen(choices[i]));
        _currentChoiceIndex = 0;
    }

    void SetCurrentChoiceIndex(uint8_t index) { _currentChoiceIndex = index; };
    bool RotaryIncrement(int8_t steps);
    bool Click(bool *focus, uint8_t *page);
    bool LongClick(bool *focus, uint8_t *page);

    virtual const char *GetLabel();
    virtual uint8_t GetCursorOffset(bool focus);

    bool (*_onChange)(int8_t);
    bool switchOnClick;

protected:
    char **_choices;
    uint8_t _currentChoiceIndex = -1;
    uint8_t _choiceCount = 0;
    uint8_t _choiceMaxLength = 0;
};
#endif