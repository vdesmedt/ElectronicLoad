#ifndef MenuItem_h_
#define MenuItem_h_

#include <Arduino.h>
#include <LiquidCrystal_PCF8574.h>

#define UM_PREFIX 1 << 0
#define UM_SUFFIX 1 << 1

class MenuItem
{
public:
    enum cursorType
    {
        Normal,
        Blink,
    };

    MenuItem(uint8_t cursorX, uint8_t cursorY);
    void SetPrefix(const char *prefix)
    {
        _prefix = prefix;
        _updateMask |= UM_PREFIX;
    }
    void SetSuffix(const char *suffix)
    {
        _suffix = suffix;
        _updateMask |= UM_SUFFIX;
    }

    virtual const char *GetPrefix() { return _prefix ? _prefix : ""; }
    virtual const char *GetSufix() { return _suffix ? _suffix : ""; }
    virtual const char *GetLabel() = 0;
    virtual uint8_t GetCursorOffset(bool focus) { return 0; }
    virtual enum cursorType GetCursorType(bool focus) { return focus ? Blink : Normal; }

    // returns true if Print is needed
    virtual bool RotaryIncrement(int8_t steps) { return true; };
    virtual bool Click(bool *focus, uint8_t *page) { return false; };
    virtual bool LongClick(bool *focus, uint8_t *page) { return false; };

    uint8_t cursorX = 0;
    uint8_t cursorY = 0;

protected:
    const char *_prefix = NULL;
    const char *_suffix = NULL;
    uint8_t _updateMask = 0;
};

#endif