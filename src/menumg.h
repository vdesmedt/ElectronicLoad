#include <Settings.h>
#include "header.h"
#include <Menu.h>

extern Menu *menu;
extern U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI mainLcd;
MultiDigitValueMenuItem *setValueMenuItem = NULL;
MultiDigitValueMenuItem *setBattCutOffMenuItem = NULL;
MultiDigitValueMenuItem *setBattCellCountMenuItem = NULL;

extern Settings *settings;
extern uint8_t lcdRefreshMask;
extern const char *workingModes[];
extern const char *onOffChoices[];
extern const char *battTypes[];
extern const char *triggerType[];
extern const char *modeUnits[];
extern const char *loggingMode[];
extern uint8_t state;

bool menu_battCellCountChanged(int32_t newValue)
{
    if (newValue > 0 && newValue <= 8)
    {
        settings->battCellCount = newValue;
        settings->version |= 0x01; //Set Dirty
        return true;
    }
    return false;
}

bool menu_LoggingChanged(int8_t newValue)
{
    settings->loggingType = newValue;
    settings->version |= 0x01; //Set Dirty
    return true;
}

bool menu_triggerTimeChanged(int32_t newValue)
{
    if (newValue >= 10 && newValue <= 60000)
    {
        settings->triggerTimer = newValue;
        settings->version |= 0x01; //Set Dirty
        return true;
    }
    return false;
}

bool menu_triggerTypeChanged(int8_t newChoice)
{
    settings->triggerType = newChoice;
    settings->version |= 0x01; //Set Dirty
    return true;
}

bool menu_modeChanged(int8_t newMode)
{
    LoadOff();
    settings->mode = newMode;
    setValueMenuItem->SetValue(settings->setValues[settings->mode]);
    setValueMenuItem->SetSuffix(modeUnits[newMode]);
    switch (settings->mode)
    {
    case MODE_CC:
    case MODE_BA:
        setValueMenuItem->SetPrecision(3);
        break;
    case MODE_CR:
    case MODE_CP:
        setValueMenuItem->SetPrecision(1);
        break;
    }

    if (newMode == MODE_BA)
    {
        setBattCellCountMenuItem->Show();
    }
    else
    {
        setBattCellCountMenuItem->Hide();
        state &= ~STATE_SUSPICIOUS_CELL_COUNT;
        lcdRefreshMask |= UM_ALERT;
    }

    settings->version |= 0x01; //Set Dirty
    return true;
}
bool menu_setValueChanged(int32_t newValue)
{
    if (newValue < 0)
        return false;
    if (newValue > 30000)
        return false;
    settings->setValues[settings->mode] = newValue;
    settings->version |= 0x01;
    return true;
}

void menu_pageChanged(uint8_t newPageIndex, uint8_t scrollLevel)
{
}

bool menu_BackLightChanged(int8_t newValue)
{
    settings->backlight = newValue;
    settings->version |= 0x01;
    return true;
}

bool menu_FanOnTemp1Changed(int32_t newValue)
{
    if (newValue <= settings->fanTemps[1] && newValue >= 0)
    {
        settings->fanTemps[0] = newValue;
        settings->version |= 0x01;
        return true;
    }
    return false;
}
bool menu_FanOnTemp2Changed(int32_t newValue)
{
    if (newValue <= settings->fanTemps[2] && newValue >= settings->fanTemps[0])
    {
        settings->fanTemps[1] = newValue;
        settings->version |= 0x01;
        return true;
    }
    return false;
}
bool menu_FanOnTemp3Changed(int32_t newValue)
{
    if (newValue <= 1000 && newValue >= settings->fanTemps[1])
    {
        settings->fanTemps[2] = newValue;
        settings->version |= 0x01;
        return true;
    }
    return false;
}

bool menu_FanHysteresisChanged(int32_t newValue)
{
    if (newValue >= 0 && newValue < 100)
    {
        settings->fanHysteresis = newValue;
        settings->version |= 0x01;
        return true;
    }
    else if (newValue < 0 || newValue > 100)
    {
        settings->fanHysteresis = 10;
    }
    return false;
}

bool menu_R17Changed(int32_t newValue)
{
    if (newValue > 900 && newValue < 1100)
    {
        settings->r17Value = newValue;
        settings->version |= 0x01;
        return true;
    }
    return false;
}

bool menu_BattTypeChanged(int8_t newValue)
{
    if (newValue >= 0 && newValue < BATT_TYPE_COUNT)
    {
        settings->battType = newValue;
        settings->version |= 0x01;
        setBattCutOffMenuItem->SetValue(settings->battCutOff[settings->battType]);
        return true;
    }
    return false;
}

bool menu_cutOffChanged(int32_t newValue)
{
    if (newValue >= 0 && newValue <= 5000)
    {
        settings->battCutOff[settings->battType] = newValue;
        settings->version |= 0x01;
        return true;
    }
    return false;
}

bool menu_loggingIntervalChanged(int32_t newValue)
{
    if (newValue >= 1 && newValue <= 3600)
    {
        settings->loggingInterval = newValue;
        settings->version |= 0x01;
        return true;
    }
    return false;
}

void setupMenu()
{
    MultiDigitValueMenuItem *vmi;
    MultiChoiceMenuItem *cmi;
    uint8_t yshift = 10;
    uint8_t y = 0;
    menu = new Menu(4, 20);
    //Page 0: Main Page
    menu->AddPage();
    cmi = menu->AddMultiChoice(workingModes, WORKINGMODE_COUNT, 24, 12, menu_modeChanged, false);
    cmi->currentChoiceIndex = settings->mode;

    setBattCellCountMenuItem = menu->AddValue(settings->battCellCount, 1, 0, 42, 12, menu_battCellCountChanged);
    setBattCellCountMenuItem->SetSuffix("\x07");

    setValueMenuItem = menu->AddValue(settings->setValues[settings->mode], 6, 3, 0, 64, menu_setValueChanged);
    setValueMenuItem->SetPrefix(F("Set:"));
    setValueMenuItem->SetSuffix(modeUnits[settings->mode]);

    menu->AddGoToPage(1, "[Conf]", 80, 64);
    //Page 1: Settings
    menu->AddPage();
    menu->AddGoToPage(0, "[\x01Main]", 0, y += yshift);

    cmi = menu->AddMultiChoice(loggingMode, LOGGIN_MODE_COUNT, 0, y += yshift, menu_LoggingChanged, true);
    cmi->SetPrefix(F("Logging     :"));
    cmi->currentChoiceIndex = settings->loggingType;

    vmi = menu->AddValue(settings->loggingInterval, 5, 0, 0, y += yshift, menu_loggingIntervalChanged);
    vmi->SetPrefix(F("Log. Interv.:"));

    cmi = menu->AddMultiChoice(triggerType, TRIGGER_TYPE_COUNT, 0, y += yshift, menu_triggerTypeChanged, true);
    cmi->SetPrefix(F("Trigger Type:"));
    cmi->currentChoiceIndex = settings->triggerType;

    vmi = menu->AddValue(settings->triggerTimer, 6, 3, 0, y += yshift, menu_triggerTimeChanged);
    vmi->SetPrefix(F("Trg Timer:"));

    cmi = menu->AddMultiChoice(battTypes, BATT_TYPE_COUNT, 0, y += yshift, menu_BattTypeChanged, true);
    cmi->SetPrefix(F("Battery Type:"));
    cmi->currentChoiceIndex = settings->battType;

    setBattCutOffMenuItem = menu->AddValue(settings->battCutOff[settings->battType], 5, 3, 0, y += yshift, menu_cutOffChanged);
    setBattCutOffMenuItem->SetPrefix(F("Batt Cut Off:"));
    setBattCutOffMenuItem->SetSuffix("V");
    setBattCutOffMenuItem->DigitIndex = 1;

    menu->AddGoToPage(2, "[Finetune]", 0, y += yshift);

    //Page 2: Finetune
    menu->AddPage();
    y = 0;
    menu->AddGoToPage(1, "[\x01Settings]", 0, y += yshift);

    vmi = menu->AddValue(settings->r17Value, 5, 1, 0, y += yshift, menu_R17Changed);
    vmi->SetPrefix(F("R17 Value   :"));
    vmi->SetSuffix("m\xF4");

    vmi = menu->AddValue(settings->fanTemps[0], 4, 1, 0, y += yshift, menu_FanOnTemp1Changed);
    vmi->SetPrefix(F("Fan Level 1 :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    vmi = menu->AddValue(settings->fanTemps[1], 4, 1, 0, y += yshift, menu_FanOnTemp2Changed);
    vmi->SetPrefix(F("Fan Level 2 :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    vmi = menu->AddValue(settings->fanTemps[2], 4, 1, 0, y += yshift, menu_FanOnTemp3Changed);
    vmi->SetPrefix(F("Cut Off     :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    vmi = menu->AddValue(settings->fanHysteresis, 3, 1, 0, y += yshift, menu_FanHysteresisChanged);
    vmi->SetPrefix(F("Hysteresis  :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    cmi = menu->AddMultiChoice(onOffChoices, 2, 0, y += yshift, menu_BackLightChanged, true);
    cmi->currentChoiceIndex = settings->backlight;
    cmi->SetPrefix(F("Backlight   :"));

    menu->Configure(&mainLcd, menu_pageChanged);
}