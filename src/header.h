#ifndef _HEADER_H_
#define _HEADER_H_

// BEGIN PIN Config
#if DEBUG_BOARD
#define P_ENC1 4
#define P_ENC2 3
#define P_ENCBTN 5
#define P_LOADON_BTN 8
#define P_LOADON_LED 7
#define P_LM35 A6
#define P_FAN 6
#else
#define P_ENC1 4
#define P_ENC2 3
#define P_ENCBTN 7
#define P_LOADON_BTN 6
#define P_LOADON_LED 9
#define P_LM35 A6
#define P_FAN 5
#endif
#define P_FLASH_SS 8
#define P_TRIGGER A3
//END PIN Confit

#define FLASH_ADR 0x20000 //First memory availble after reserved for OTA update

#define LCD_TEMP_MAX_UPDATE_RATE 1000
#define UM_TEMP 1 << 0
#define UM_VOLTAGE 1 << 1
#define UM_CURRENT 1 << 2
#define UM_POWER 1 << 3
#define UM_LOAD_ONOFF 1 << 4
#define UM_TIME 1 << 5
#define UM_MAH 1 << 6
#define UM_CELLCOUNT 1 << 7

#define BATT_TYPE_COUNT 4
#define WORKINGMODE_COUNT 4
#define MODE_CC 0
#define MODE_CR 1
#define MODE_CP 2
#define MODE_BA 3

void timerIsr();
void setupMenu();
void setOutput();
void SetBacklight();
void refreshDisplay();
void loadButton_click();
void loadButton_longClick();
void selfTest();
void setupLoadButton();
void setupLoadButton();
void SaveSettings();

#endif