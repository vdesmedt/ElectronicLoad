#include <Settings.h>
#include <LiquidCrystal_PCF8574.h>
#include "header.h"
#include <Menu.h>

Menu *menu;
MultiDigitValueMenuItem *setValueMenuItem = NULL;
MultiDigitValueMenuItem *setBattCutOffMenuItem = NULL;
MultiDigitValueMenuItem *setBattCellCountMenuItem = NULL;

extern Settings *settings;
extern uint8_t lcdRefreshMask;
extern LiquidCrystal_PCF8574 lcd1;
extern const char *workingModes[];
extern const char *onOffChoices[];
extern const char *battTypes[];
extern const char *triggerType[];
extern const char *modeUnits[];
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
    return true;
}

bool menu_triggerTypeChanged(int8_t newTriggerType)
{
    return true;
}

bool menu_modeChanged(int8_t newMode)
{
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
        menu->PrintItem(setBattCellCountMenuItem);
    }
    else
    {
        setBattCellCountMenuItem->Hide();
        lcd1.setCursor(7, 0);
        lcd1.print(F("  "));
        menu->PrintCursor();
        state &= ~STATE_SUSPICIOUS_CELL_COUNT;
        lcdRefreshMask |= UM_ALERT;
    }

    menu->PrintItem(setValueMenuItem);
    LoadOff();
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
    lcd1.clear();
    switch (newPageIndex)
    {
    case 0:
        lcd1.setCursor(18, 0);
        lcd1.print((char)0xDF);

        lcd1.setCursor(5, 1);
        lcd1.print(F("A"));
        lcd1.setCursor(13, 1);
        lcd1.print(F("V"));
        lcd1.setCursor(19, 1);
        lcd1.print(F("W"));
        lcd1.setCursor(17, 3);
        lcd1.print(F("mAh"));
        lcd1.setCursor(0, 3);
        lcd1.write(SC_WATCH);
        lcdRefreshMask = 0xFF;
        SaveSettings();
        break;
    }
}

bool menu_BackLightChanged(int8_t newValue)
{
    settings->backlight = newValue;
    settings->version |= 0x01;
    SetBacklight();
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
    if (newValue >= 1 & newValue <= 3600)
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
    uint8_t y = 0;
    menu = new Menu(4, 20);
    //Page 0: Main Page
    menu->AddPage();
    cmi = menu->AddMultiChoice(workingModes, WORKINGMODE_COUNT, 4, 0, menu_modeChanged, false);
    cmi->currentChoiceIndex = settings->mode;

    setBattCellCountMenuItem = menu->AddValue(settings->battCellCount, 1, 0, 7, 0, menu_battCellCountChanged);
    setBattCellCountMenuItem->SetSuffix("\x07");

    setValueMenuItem = menu->AddValue(settings->setValues[settings->mode], 6, 3, 0, 2, menu_setValueChanged);
    setValueMenuItem->SetPrefix(F("Set:"));
    setValueMenuItem->SetSuffix(modeUnits[settings->mode]);

    menu->AddGoToPage(1, "[Conf]", 14, 2);
    //Page 1: Settings
    menu->AddPage();
    menu->AddGoToPage(0, "[\x01Main]", 0, y++);

    cmi = menu->AddMultiChoice(onOffChoices, 2, 0, y++, menu_LoggingChanged, true);
    cmi->SetPrefix(F("Logging     :"));
    cmi->currentChoiceIndex = settings->loggingType;

    vmi = menu->AddValue(settings->loggingInterval, 5, 0, 0, y++, menu_loggingIntervalChanged);
    vmi->SetPrefix(F("Log. Interv.:"));

    cmi = menu->AddMultiChoice(triggerType, TRIGGER_TYPE_COUNT, 0, y++, menu_triggerTypeChanged, true);
    cmi->SetPrefix(F("Trigger Type:"));
    cmi->currentChoiceIndex = settings->triggerType;

    vmi = menu->AddValue(settings->triggerTimer, 6, 3, 0, y++, menu_triggerTimeChanged);
    vmi->SetPrefix(F("Trg Timer:"));

    cmi = menu->AddMultiChoice(battTypes, BATT_TYPE_COUNT, 0, y++, menu_BattTypeChanged, true);
    cmi->SetPrefix(F("Battery Type:"));
    cmi->currentChoiceIndex = settings->battType;

    setBattCutOffMenuItem = menu->AddValue(settings->battCutOff[settings->battType], 5, 3, 0, y++, menu_cutOffChanged);
    setBattCutOffMenuItem->SetPrefix(F("Batt Cut Off:"));
    setBattCutOffMenuItem->SetSuffix("V");
    setBattCutOffMenuItem->DigitIndex = 1;

    menu->AddGoToPage(2, "[Finetune]", 0, y++);

    //Page 2: Finetune
    menu->AddPage();
    y = 0;
    menu->AddGoToPage(1, "[\x01Settings]", 0, y++);

    vmi = menu->AddValue(settings->r17Value, 5, 1, 0, y++, menu_R17Changed);
    vmi->SetPrefix(F("R17 Value   :"));
    vmi->SetSuffix("m\xF4");

    vmi = menu->AddValue(settings->fanTemps[0], 4, 1, 0, y++, menu_FanOnTemp1Changed);
    vmi->SetPrefix(F("Fan Level 1 :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    vmi = menu->AddValue(settings->fanTemps[1], 4, 1, 0, y++, menu_FanOnTemp2Changed);
    vmi->SetPrefix(F("Fan Level 2 :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    vmi = menu->AddValue(settings->fanTemps[2], 4, 1, 0, y++, menu_FanOnTemp3Changed);
    vmi->SetPrefix(F("Cut Off     :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    vmi = menu->AddValue(settings->fanHysteresis, 3, 1, 0, y++, menu_FanHysteresisChanged);
    vmi->SetPrefix(F("Hysteresis  :"));
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    cmi = menu->AddMultiChoice(onOffChoices, 2, 0, y++, menu_BackLightChanged, true);
    cmi->currentChoiceIndex = settings->backlight;
    cmi->SetPrefix(F("Backlight   :"));

    menu->Configure(&lcd1, menu_pageChanged);
}