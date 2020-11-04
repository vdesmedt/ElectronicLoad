#ifndef MenuItem_h_
#define MenuItem_h_

#include <Arduino.h>
#include <U8g2lib.h>

#define IS_SHOW 1

class MenuItem
{
public:
    enum cursorType
    {
        Normal,
        Blink,
    };

    MenuItem(uint8_t cursorX, uint8_t cursorY);
    void SetPrefix(const __FlashStringHelper *prefix);
    void SetSuffix(const char *suffix);

    virtual const __FlashStringHelper *GetPrefix() { return _prefix ? _prefix : F(""); }
    virtual const char *GetSufix() { return _suffix ? _suffix : ""; }
    virtual const char *GetLabel() = 0;
    virtual uint8_t GetCursorOffset(bool focus) { return 0; }
    virtual enum cursorType GetCursorType(bool focus) { return focus ? Blink : Normal; }

    // returns true if Print is needed
    virtual bool RotaryIncrement(int8_t steps) { return true; };
    virtual bool Click(bool *focus, uint8_t *page) { return false; };
    virtual bool LongClick(bool *focus, uint8_t *page) { return false; };
    virtual void Print(U8G2 *lcd, uint8_t y, bool current, bool hasFocus);
    virtual uint8_t PrintLabel(U8G2 *lcd, uint8_t x, uint8_t y, bool current, bool hasFocus);

    virtual void Hide() { _state &= ~IS_SHOW; }
    virtual void Show();
    virtual bool IsShown();

    uint8_t getCx() { return _cursorX; }
    uint8_t getCy() { return _cursorY; }
    void setCx(uint8_t x) { _cursorX = x; }
    void setCy(uint8_t y) { _cursorY = y; }

protected:
    const __FlashStringHelper *_prefix = NULL;
    uint8_t _prefixLenth = 0;
    const char *_suffix = NULL;
    uint8_t _state = IS_SHOW;
    uint8_t *_labelFont = NULL;

private:
    uint8_t _cursorX = 0;
    uint8_t _cursorY = 0;
};

#endif