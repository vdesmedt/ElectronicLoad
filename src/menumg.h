#include <Settings.h>
#include <LiquidCrystal_PCF8574.h>
#include "header.h"
#include <Menu.h>

Menu *menu;
MultiDigitValueMenuItem *setValueMenuItem = NULL;
MultiDigitValueMenuItem *setBattCutOffMenuItem = NULL;

extern Settings *settings;
extern uint8_t lcdRefreshMask;
extern bool swLoadOnOff;
extern LiquidCrystal_PCF8574 lcd1;

const char *workingModes[WORKINGMODE_COUNT] = {"CC", "CR", "CP", "Ba"};
const char *onOffChoices[] = {"On ", "Off"};
const char *modeUnits[WORKINGMODE_COUNT] = {"A", "\xF4", "W", "A"};
const char *battTypes[BATT_TYPE_COUNT] = {"LiPo", "NiMh"};

bool menu_modeChanged(int8_t newMode)
{
    settings->mode = newMode;
    lcdRefreshMask |= UM_CELLCOUNT;
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
    menu->PrintItem(setValueMenuItem);
    swLoadOnOff = false;
    lcdRefreshMask |= UM_LOAD_ONOFF;
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
        lcd1.print("A");
        lcd1.setCursor(13, 1);
        lcd1.print("V");
        lcd1.setCursor(19, 1);
        lcd1.print("W");
        lcd1.setCursor(17, 3);
        lcd1.print("mAh");
        lcdRefreshMask = ~0;
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
    if (newValue >= 0 && newValue <= 50)
    {
        settings->battCutOff[settings->battType] = newValue;
        settings->version |= 0x01;
        return true;
    }
    return false;
}

void setupMenu()
{
    MultiDigitValueMenuItem *vmi;
    MultiChoiceMenuItem *cmi;

    menu = new Menu(3, 20);
    //Page 0: Main Page
    menu->AddPage();
    cmi = menu->AddMultiChoice(workingModes, WORKINGMODE_COUNT, 4, 0, menu_modeChanged, false);
    cmi->currentChoiceIndex = settings->mode;
    setValueMenuItem = menu->AddValue(settings->setValues[settings->mode], 6, 3, 0, 2, menu_setValueChanged);
    setValueMenuItem->SetPrefix("Set:");
    setValueMenuItem->SetSuffix(modeUnits[settings->mode]);

    menu->AddGoToPage(1, "[Conf]", 14, 2);
    //Page 1: Settings
    menu->AddPage();
    menu->AddGoToPage(0, "[Main]", 0, 0);
    cmi = menu->AddMultiChoice(onOffChoices, 2, 0, 1, menu_BackLightChanged, true);
    cmi->currentChoiceIndex = settings->backlight;
    cmi->SetPrefix("Backlight   :");

    vmi = menu->AddValue(settings->r17Value, 5, 1, 0, 2, menu_R17Changed);
    vmi->SetPrefix("R17 Value   :");
    vmi->SetSuffix("m\xF4");
    cmi = menu->AddMultiChoice(battTypes, 2, 0, 3, menu_BattTypeChanged, true);
    cmi->SetPrefix("Battery Type:");
    cmi->currentChoiceIndex = settings->battType;

    setBattCutOffMenuItem = menu->AddValue(settings->battCutOff[settings->battType], 4, 1, 0, 4, menu_cutOffChanged);
    setBattCutOffMenuItem->SetPrefix("Batt Cut Off:");
    setBattCutOffMenuItem->SetSuffix("V");
    setBattCutOffMenuItem->DigitIndex = 1;

    menu->AddGoToPage(2, "[Fan & Temp]", 0, 5);

    //Page 2: Fan Management
    menu->AddPage();
    menu->AddGoToPage(1, "[Settings]", 0, 0);
    vmi = menu->AddValue(settings->fanTemps[0], 4, 1, 0, 1, menu_FanOnTemp1Changed);
    vmi->SetPrefix("Level 1 :");
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;
    vmi = menu->AddValue(settings->fanTemps[1], 4, 1, 0, 2, menu_FanOnTemp2Changed);
    vmi->SetPrefix("Level 2 :");
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;
    vmi = menu->AddValue(settings->fanTemps[2], 4, 1, 0, 3, menu_FanOnTemp3Changed);
    vmi->SetPrefix("Cut Off :");
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;
    vmi = menu->AddValue(settings->fanHysteresis, 3, 1, 0, 4, menu_FanHysteresisChanged);
    vmi->SetPrefix("Hysteresis:");
    vmi->SetSuffix("\xDF\x43");
    vmi->DigitIndex = 1;

    menu->Configure(&lcd1, menu_pageChanged);
}