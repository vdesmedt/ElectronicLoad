#define DEBUG_BOARD_
#define DEBUG_TIMINGS true

#define MCP4725_EXTENDED
#include <Wire.h>
#include <Arduino.h>
#include <ClickEncoder.h>
#include <OneButton.h>
#include <TimerOne.h>
#include <LiquidCrystal_PCF8574.h>
#include <MCP4725.h>
#include <MCP342x.h>
#include <Menu.h>
#include <Statistic.h>
#include <Filters/SMA.hpp>
#include <SPIFlash.h>
#include <MemoryFree.h>
#include <debug.h>
#include <MCP79410_Timer.h>
#include <specialLcdChar.h>
#include "pindef.h"

//BEGIN ADC-DAC
#ifdef DEBUG_BOARD
#define DAC_ADDR 0x60
#else
#define DAC_ADDR 0x61
#endif
#define ADC_ADDR 0x68
#define RTC_ADDR 0x6F
#define IOE_ADDR 0x20
#define DAC_REF_VOLTAGE 3300

MCP4725 dac(DAC_ADDR);
MCP342x adc = MCP342x(ADC_ADDR);
MCP79410_Timer _rtcTimer(RTC_ADDR);
//END ADC-DAC

//BEGIN - LCD ###
LiquidCrystal_PCF8574 lcd1(0x27);
#define LCD_TEMP_MAX_UPDATE_RATE 200
#define UM_TEMP 1 << 0
#define UM_VOLTAGE 1 << 1
#define UM_CURRENT 1 << 2
#define UM_POWER 1 << 3
#define UM_LOAD_ONOFF 1 << 4
#define UM_TIME 1 << 5
#define UM_MAH 1 << 6
#define UM_CELLCOUNT 1 << 7
uint8_t lcdRefreshMask = ~0;

#define WORKINGMODE_COUNT 4
#define MODE_CC 0
#define MODE_CR 1
#define MODE_CP 2
#define MODE_BA 3
const char *workingModes[WORKINGMODE_COUNT] = {"CC", "CR", "CP", "BA"};
const char *onOffChoices[] = {"On ", "Off"};
const char *modeUnits[WORKINGMODE_COUNT] = {"A", "O", "W", "A"};
#define BATT_TYPE_COUNT 2
const char *battTypes[BATT_TYPE_COUNT] = {"Lipo", "NiMh"};
const float battTypicalCellVoltage[BATT_TYPE_COUNT] = {3700, 1100};
Menu *menu;
MultiDigitValueMenuItem *setValueMenuItem = NULL;
MultiDigitValueMenuItem *setBattCutOffMenuItem = NULL;
//END - LCD ###

//BEGIN Flash
#define FLASH_ADR 0x20000 //First memory availble after reserved for OTA update
#define SETTINGS_VERSION 0x03
struct Settings
{
  uint8_t version = (SETTINGS_VERSION << 1) + 0;              //LSB to 1 means dirty
  uint8_t mode = 0;                                           //CC
  uint16_t setValues[WORKINGMODE_COUNT] = {0, 1000, 50, 100}; //0mA 100Ω, 5W, 100mA
  uint8_t backlight = 0;                                      //On
  uint16_t r17Value = 1000;                                   //100mΩ
  uint8_t battType = 0;                                       //Lipo
  uint16_t battCutOff[2] = {32, 10};                          //3.2V 1.0V
  uint16_t fanTemps[3] = {300, 400, 600};                     //30°C
  uint8_t fanHysteresis = 10;                                 //1°C
};
struct Settings *settings = new Settings();

SPIFlash flash(P_FLASH_SS, 0xEF30);
//END FLash

//BEGIN - Encoder ###
ClickEncoder *encoder;
#ifdef DEBUG_BOARD
#define ENC_STEPS 2
#else
#define ENC_STEPS 4
#endif
//END - Encoder ###

//BEGIN - Load ###
uint8_t battCellCount = 1;
uint8_t fanLevel = 0;
uint16_t readTemperature = 0; // 1/10 °C
int16_t readCurrent = 0;      // mA
int16_t readVoltage = 0;      // mV
bool swLoadOnOff = false;
double totalmAh = 0;
//END - Load ###

OneButton *pushButton;

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

void timerIsr()
{
  encoder->service();
}

void LoadOn()
{
  swLoadOnOff = true;
  digitalWrite(P_LOADON_LED, HIGH);
  lcdRefreshMask |= UM_LOAD_ONOFF;
  setOutput();
  _rtcTimer.start();
}

void LoadOff()
{
  swLoadOnOff = false;
  digitalWrite(P_LOADON_LED, LOW);
  lcdRefreshMask |= UM_LOAD_ONOFF;
  setOutput();
  _rtcTimer.stop();
}

void setOutput()
{
  if (swLoadOnOff)
  {
    double newDacValue = 0; //Output in mV
    double set = settings->setValues[settings->mode];
    switch (settings->mode)
    {
    case MODE_CC:
    case MODE_BA:
      newDacValue = set;
      break;
    case MODE_CR:
      newDacValue = (double)readVoltage / (set / 10); //mV / Ω
      break;
    case MODE_CP:
      newDacValue = 1000 * (set * 100) / readVoltage;
      break;
    }

    //Adjust for R17 Value
    newDacValue = newDacValue * (double)settings->r17Value / 1000.0;
    debug_printb(F("New DAC Value:"), "%d\n", (int)round(newDacValue));
    dac.setValue(round(newDacValue));
  }
  else
  {
    dac.setValue(0);
  }
}

void SetBacklight()
{
  lcd1.setBacklight(settings->backlight == 0 ? 255 : 0);
}

void refreshDisplay()
{
#if DEBUG_TIMINGS
  static Statistic drawStats;
  static unsigned long loopStart = 0;
  loopStart = micros();
#endif
  static char buffer[10];
  if (menu->GetCurrentPage() == 0)
  {
    //Load On Off
    if (lcdRefreshMask & UM_LOAD_ONOFF)
    {
      lcd1.setCursor(0, 0);
      lcd1.print(swLoadOnOff ? "On :" : "Off:");
      lcdRefreshMask &= ~UM_LOAD_ONOFF;
      menu->PrintCursor();
    }

    //Bat cell Count
    if (lcdRefreshMask & UM_CELLCOUNT)
    {
      lcd1.setCursor(7, 0);
      if (settings->mode == MODE_BA)
      {
        lcd1.print(battCellCount);
        lcd1.print("x");
        lcd1.print(battTypes[settings->battType]);
      }
      else
      {
        lcd1.print("      ");
      }
      lcdRefreshMask &= ~UM_CELLCOUNT;
      menu->PrintCursor();
    }

    //Temp
    if (lcdRefreshMask & UM_TEMP)
    {
      lcd1.setCursor(13, 0);
      lcd1.print(dtostrf((double)readTemperature / 10, 5, 1, buffer));
      lcd1.setCursor(19, 0);
      lcd1.write(SC_THERMO_L0 + fanLevel);
      lcdRefreshMask &= ~UM_TEMP;
      menu->PrintCursor();
    }

    //Readings
    if (lcdRefreshMask & UM_CURRENT)
    {
      lcd1.setCursor(0, 1);
      lcd1.print(dtostrf((double)readCurrent / 1000, 5, 3, buffer));
      lcdRefreshMask &= ~UM_CURRENT;
      menu->PrintCursor();
    }
    if (lcdRefreshMask & UM_VOLTAGE)
    {
      lcd1.setCursor(7, 1);
      lcd1.print(dtostrf((double)readVoltage / 1000, 5, 3, buffer));
      lcdRefreshMask &= ~UM_VOLTAGE;
      menu->PrintCursor();
    }
    if (lcdRefreshMask & UM_POWER)
    {
      lcd1.setCursor(15, 1);
      lcd1.print(dtostrf((double)readVoltage * readCurrent / 1000000, 4, 1, buffer));
      lcdRefreshMask &= ~UM_POWER;
      menu->PrintCursor();
    }

    //Time
    static unsigned long lastTotalSec = 0;
    static unsigned long lastTmUpdate = 0;
    if (lcdRefreshMask & UM_TIME || (lastTmUpdate + 200 < millis() && lastTotalSec != _rtcTimer.getTotalSeconds()))
    {
      static char buffer[10];
      lcd1.setCursor(0, 3);
      _rtcTimer.getTime(buffer);
      lcd1.print(buffer);
      lcd1.setCursor(10, 3);
      dtostrf(totalmAh / 3600000, 7, 1, buffer);
      lcd1.print(buffer);

      menu->PrintCursor();
      lastTotalSec = _rtcTimer.getTotalSeconds();
      lastTmUpdate = millis();
      lcdRefreshMask &= ~UM_TIME;
    }
  }
#if DEBUG_TIMINGS
  drawStats.add(micros() - loopStart);
  if (DEBUG_TIMINGS && drawStats.count() == 5000)
  {
    Serial.print("RefeshLcd:");
    Serial.print((uint16_t)drawStats.average());
    Serial.print(" / ");
    Serial.print((uint16_t)drawStats.minimum());
    Serial.print("->");
    Serial.print((uint16_t)drawStats.maximum());
    Serial.print(" / ");
    Serial.println((uint16_t)drawStats.variance());
    drawStats.clear();
  }
#endif
}

void loadButton_click()
{
  swLoadOnOff = !swLoadOnOff;
  if (swLoadOnOff)
    LoadOn();
  else
    LoadOff();
  SaveSettings();
}

void loadButton_longClick()
{
  if (!swLoadOnOff)
  {
    _rtcTimer.reset();
    totalmAh = 0;
  }
}

void selfTest()
{
  uint8_t selfTestResult = 0;
  lcd1.home();
  lcd1.print(F("= Electronic  Load ="));
  lcd1.setCursor(0, 1);
  lcd1.print(F("ADC DAC RTC IOE FLH"));

  //Check ADC Presence
  Wire.requestFrom(ADC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x01;

  //Check DAC Presence
  Wire.requestFrom(DAC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x02;

  //Check RTC Presence
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
  Wire.requestFrom(RTC_ADDR, (uint8_t)1);
  if (!Wire.available())
    selfTestResult |= 0x04;

  //Check IOX
  Wire.beginTransmission(IOE_ADDR);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
  Wire.requestFrom(IOE_ADDR, (uint8_t)1);
  if (!Wire.available())
    selfTestResult |= 0x08;

  //Check Flash
  if (!flash.initialize())
    selfTestResult |= 0x10;

  //Result ?
  for (uint8_t i = 0; i < 5; i++)
  {
    lcd1.setCursor(4 * i + 1, 2);
    lcd1.print(selfTestResult & (1 << i) ? "X" : "V");
  }
  delay(selfTestResult > 0 ? 2000 : 500);

  //Booting
  debug_print("Self test done\n");
}

//TODO : Auto Gain adjustment : For low voltage, 4x will increase resolution
//TODO : Switch to continuous convertion
SMA<10, uint16_t, uint32_t> temperatureSMA;
void actuateReadings()
{
  static MCP342x::Config ch1Config(MCP342x::channel1, MCP342x::oneShot, MCP342x::resolution16, MCP342x::gain1);
  static MCP342x::Config ch2Config(MCP342x::channel2, MCP342x::oneShot, MCP342x::resolution16, MCP342x::gain4);
  static MCP342x::Config adcStatus;
  static long adcValue;
  static uint8_t adcErr;
  static int8_t runConvertCh = 1;

  //Temperature Reading

  static unsigned long lastTempRead = 0;
  if (lastTempRead + LCD_TEMP_MAX_UPDATE_RATE < millis())
  {
    readTemperature = temperatureSMA(analogRead(P_LM35)) * 4.83871;
    lcdRefreshMask |= UM_TEMP;
    lastTempRead = millis();
  }

  //Voltage & Current Reading (alternate)
  if (runConvertCh == 1)
  {
    adcErr = adc.convert(ch1Config);
    if (adcErr)
      debug_printb(F("ADC-Ch1 Conv Err:"), "%i\n", adcErr);
    runConvertCh = -1;
  }
  if (runConvertCh == 2)
  {
    adcErr = adc.convert(ch2Config);
    if (adcErr)
      debug_printb(F("ADC-Ch2 Conv Err:"), "%i\n", adcErr);
    runConvertCh = -2;
  }

  if (runConvertCh == -1)
  {
    static int16_t newVoltage = 0;
    adcErr = adc.read(adcValue, adcStatus);
    if (adcStatus.isReady())
    {
      if (adcErr)
      {
        debug_printb(F("ADC-Ch1 Read Err:"), "%i\n", adcErr);
      }
      else
      {
        if (adcValue < 0)
          adcValue = 0;
        static unsigned long lastVoltageReading = 0;
        newVoltage = adcValue * (2048.0 / 32768) * 50;
        if (newVoltage != readVoltage || lastVoltageReading + 500 < millis())
        {
          if (settings->mode == MODE_BA)
          {
            uint8_t bcc = round((float)newVoltage / battTypicalCellVoltage[settings->battType]);
            if (bcc != battCellCount)
            {
              battCellCount = bcc;
              lcdRefreshMask |= UM_CELLCOUNT;
            }
          }
          readVoltage = newVoltage;
          lcdRefreshMask |= (UM_VOLTAGE | UM_POWER);
        }
      }
      runConvertCh = 2;
    }
  }

  if (runConvertCh == -2)
  {
    static int16_t newCurrent = 0;
    adcErr = adc.read(adcValue, adcStatus);
    if (adcStatus.isReady())
    {
      if (adcErr)
      {
        debug_printb(F("ADC-Ch2 Read Err:"), "%i\n", adcErr);
      }
      else
      {
        if (adcValue < 0)
          adcValue = 0;
        static unsigned long lastCurrentReading = 0;
        newCurrent = adcValue * (2048.0 / 32768) * 2.5 * (1000.0 / settings->r17Value);
        if (newCurrent != readCurrent || lastCurrentReading + 500 < millis())
        {
          if (swLoadOnOff)
          {
            totalmAh += readCurrent * (millis() - lastCurrentReading);
          }
          readCurrent = newCurrent;

          lcdRefreshMask |= (UM_CURRENT | UM_POWER);
          lastCurrentReading = millis();
        }
      }
      runConvertCh = 1;
    }
  }
}

//TODO Adjust PWM's from settings (need scrollable menu)
void adjustFanSpeed()
{
  static uint16_t fanLevelPwm[] = {0, 900, 1023};
  switch (fanLevel)
  {
  case 0:
    if (readTemperature > settings->fanTemps[0] + settings->fanHysteresis)
      fanLevel = 1;
    break;
  case 1:
    if (readTemperature < settings->fanTemps[0] - settings->fanHysteresis)
      fanLevel = 0;
    else if (readTemperature > settings->fanTemps[1] + settings->fanHysteresis)
      fanLevel = 2;
    break;
  case 2:
    if (readTemperature < settings->fanTemps[1] - settings->fanHysteresis)
      fanLevel = 1;
    break;
  }
  analogWrite(P_FAN, fanLevelPwm[fanLevel]);
}

Statistic flashClearStats;
Statistic flashSaveStats;
void SaveSettings()
{
  if (settings->version & 0x01)
  {
    unsigned long s = micros();
    flash.blockErase4K(FLASH_ADR);
    while (flash.busy())
      ;
    flashClearStats.add(micros() - s);
    settings->version &= ~1; //Set Dirty flag to zero
    s = micros();
    flash.writeBytes(FLASH_ADR, settings, sizeof(Settings));
    while (flash.busy())
      ;
    flashSaveStats.add(micros() - s);
    debug_printb(F("Flash Stats (C/S):"), "%i/%i\n", flashClearStats.average(), flashSaveStats.average());
  }
}

void setup()
{
  Serial.begin(9600);

  // initialize the lcd
  lcd1.begin(20, 4);
  lcd1.setBacklight(255);
  lcd1.createChar(SC_THERMO_L0, thermo_l0);
  lcd1.createChar(SC_THERMO_L1, thermo_l1);
  lcd1.createChar(SC_THERMO_L2, thermo_l2);
  lcd1.createChar(SC_THERMO_L3, thermo_l3);

  memset((void *)modeUnits[1], 0xF4, 1);

  pinMode(P_LM35, INPUT);
  pinMode(P_FAN, OUTPUT);
  digitalWrite(P_FAN, LOW);
  pinMode(P_LOADON_LED, OUTPUT);
  pinMode(P_TRIGGER, INPUT_PULLUP);

  //Setup Encoder
  encoder = new ClickEncoder(P_ENC1, P_ENC2, P_ENCBTN, ENC_STEPS);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  encoder->setAccelerationEnabled(true);
  encoder->setDoubleClickEnabled(false);
  encoder->setHoldTime(500);

//Setup Load button
#ifdef DEBUG_BOARD
  pushButton = new OneButton(P_LOADON_BTN, 1, true);
#else
  pushButton = new OneButton(P_LOADON_BTN, 0, false);
#endif
  pushButton->setClickTicks(400);
  pushButton->setPressTicks(600);
  pushButton->attachClick(loadButton_click);
  pushButton->attachLongPressStart(loadButton_longClick);

  MCP342x::generalCallReset();
  delay(1); // MC342x needs 300us to settle, wait 1ms

  selfTest();

  dac.begin();

  //Load Settings from Flash
  flash.readBytes(FLASH_ADR, settings, sizeof(Settings));
  if ((settings->version >> 1) != SETTINGS_VERSION)
  {
    debug_printa("Setting Ver NoMatch %X vs %X\n", settings->version >> 1, SETTINGS_VERSION);
    free(settings);
    settings = new Settings();
    //Enjoy this first booting time to setup DAC POR Value (1/2 ref voltage out of factory)
    dac.writeDAC(0, true);
    while (!dac.RDY())
      ;
  }
  SetBacklight();

  Wire.setClock(400000); //Done after dac.Begin as it itself setClock to 100.000

  setupMenu();
  menu_modeChanged(settings->mode);

  lcdRefreshMask = ~0;
}

void loop()
{
#if DEBUG_TIMINGS
  static Statistic loopStats;
  static unsigned long loopStart = 0;
  loopStart = micros();
#endif
  static uint8_t buttonState;
  static uint8_t oldState = ClickEncoder::Open;

  pushButton->tick();
  actuateReadings();
  if (readTemperature > settings->fanTemps[2])
  {
    LoadOff();
  }
  adjustFanSpeed();
  refreshDisplay();

  //Encoder mgmnt
  int16_t enc = encoder->getValue();
  if (enc != 0)
    menu->EncoderInc(enc);

  buttonState = encoder->getButton();
  if (buttonState == ClickEncoder::Clicked)
    menu->Click();
  else if (buttonState == ClickEncoder::Held && oldState != ClickEncoder::Held)
    menu->LongClick();
  oldState = buttonState;

#if DEBUG_TIMINGS
  loopStats.add(micros() - loopStart);
  if (loopStats.count() == 5000)
  {
    Serial.print("LoopStats:");
    Serial.print((uint16_t)loopStats.average());
    Serial.print(" / ");
    Serial.print((uint16_t)loopStats.minimum());
    Serial.print("->");
    Serial.print((uint16_t)loopStats.maximum());
    Serial.print(" / ");
    Serial.print((uint16_t)loopStats.variance());
    Serial.print(" / ");
    Serial.println((size_t)freeMemory());
    loopStats.clear();
  }
#endif
}