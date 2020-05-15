#include <GotoPageMenuItem.h>

const char *GoToPageMenuItem::GetLabel()
{
    return _title;
}

uint8_t GoToPageMenuItem::GetCursorOffset(bool focus)
{
    return 0;
}

bool GoToPageMenuItem::Click(bool *focus, uint8_t *page)
{
    *focus = false;
    *page = this->targetPageIndex;
    return true;
}