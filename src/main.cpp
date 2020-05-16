#define DEBUG_BOARD_

#include <Wire.h>
#include <Arduino.h>
#include <ClickEncoder.h>
#include <OneButton.h>
#include <TimerOne.h>
#include <LiquidCrystal_PCF8574.h>
#define MCP4725_EXTENDED ON
#include <MCP4725.h>
#include <MCP342x.h>
#include <RTCx.h>
#include <Menu.h>
#include <Statistic.h>
#include <Filters/SMA.hpp>
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
RTCx _rtc(RTC_ADDR);
//END ADC-DAC

//BEGIN - LCD ###
LiquidCrystal_PCF8574 lcd1(0x27);
#define LCD_TEMP_MAX_UPDATE_RATE 200
#define UM_TEMP 1 << 0
#define UM_VOLTAGE 1 << 1
#define UM_CURRENT 1 << 2
#define UM_POWER 1 << 3
#define UM_LOAD_ONOFF 1 << 4
int lcdRefreshMask = ~0;

#define WORKINGMODE_COUNT 4
const char *working_modes[WORKINGMODE_COUNT] = {"CC", "CR", "CP", "BA"};
const char *onOffChoices[] = {"On ", "Off"};
const char *modeUnits[WORKINGMODE_COUNT] = {"A", "O", "W", "A"};
#define BATT_TYPE_COUNT 2
const char *battTypes[BATT_TYPE_COUNT] = {"Lipo", "NiMh"};
Menu *menu;
MultiDigitValueMenuItem *setValueMenuItem = NULL;
//END - LCD ###

//BEGIN - Encoder ###
ClickEncoder *encoder;
#ifdef DEBUG_BOARD
#define ENC_STEPS 2
#else
#define ENC_STEPS 4
#endif
//END - Encoder ###

//BEGIN - Load ###
uint16_t r17Resistance = 1000; // 1/10 mΩ
uint8_t battType = 0;
uint8_t battCellCutOut = 32; // 1/10 V
uint8_t battCellCount = 1;
uint8_t workingMode = 0;
int16_t setValues[WORKINGMODE_COUNT] = {0, 0, 0, 0};
uint8_t fanLevel = 0;
uint16_t fanOnTemp[] = {300, 400};
uint8_t fanHysteresis = 20;

uint16_t readTemperature = 0; // 1/10 °C
int16_t readCurrent = 0;      // mA
int16_t readVoltage = 0;      // mV
bool swLoadOnOff = false;
//END - Load ###

OneButton *pushButton;

void timerIsr();
bool menu_modeChanged(int32_t newMode);
bool menu_setValueChanged(int32_t newValue);
void menu_pageChanged(uint8_t newPageIndex);
void setupMenu();
void setOutput();
void refreshDisplay();
void loadButton_click();
void loadButton_longClick();
void selfTest();
void setupLoadButton();
void setupLoadButton();

bool menu_modeChanged(int8_t newMode)
{
  Serial.print("New mode:");
  Serial.println(newMode);
  workingMode = newMode;
  setValueMenuItem->SetValue(setValues[workingMode]);
  setValueMenuItem->SetSuffix(modeUnits[newMode]);
  menu->PrintItem(setValueMenuItem);
  return true;
}

bool menu_setValueChanged(int32_t newValue)
{
  if (newValue < 0)
    return false;
  if (newValue > 30000)
    return false;
  setValues[workingMode] = newValue;
  setOutput();
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

    lcdRefreshMask = ~0;
    break;
  }
}

bool menu_BackLightChanged(int8_t newValue)
{
  lcd1.setBacklight(newValue ? 0 : 255);
  return true;
}

bool menu_FanFeedbackChanged(int8_t newValue)
{
  return true;
}

bool menu_FanOnTemp1Changed(int32_t newValue)
{
  if (newValue <= 600 && newValue >= 0)
  {
    fanOnTemp[0] = newValue;
    return true;
  }
  return false;
}
bool menu_FanOnTemp2Changed(int32_t newValue)
{
  if (newValue <= 600 && newValue >= 0)
  {
    fanOnTemp[0] = newValue;
    return true;
  }
  return false;
}

bool menu_FanHysteresisChanged(int32_t newValue)
{
  if (newValue >= 0 && newValue < 100)
  {
    fanHysteresis = newValue;
    return true;
  }
  return false;
}

bool menu_R17Changed(int32_t newValue)
{
  if (newValue > 900 && newValue < 1100)
  {
    r17Resistance = newValue;
    setOutput();
    return true;
  }
  return false;
}

bool menu_BattTypeChanged(int8_t newValue)
{
  if (newValue >= 0 && newValue < BATT_TYPE_COUNT)
  {
    battType = newValue;
    return true;
  }
  return false;
}

bool menu_cutOffChanged(int32_t newValue)
{
  if (newValue >= 0 && newValue <= 300)
  {
    battCellCutOut = newValue;
    return true;
  }
  return false;
}

bool menu_battCellCountChanged(int32_t newValue)
{
  if (newValue >= 1 && newValue <= 8)
  {
    battCellCount = newValue;
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
  cmi = menu->AddMultiChoice(working_modes, WORKINGMODE_COUNT, 4, 0, menu_modeChanged, false);
  setValueMenuItem = menu->AddValue(0, 6, 3, 0, 2, menu_setValueChanged);
  setValueMenuItem->SetPrefix("Set:");
  setValueMenuItem->SetSuffix("A");

  menu->AddGoToPage(1, "[Conf]", 14, 2);
  //Page 1: Settings
  menu->AddPage();
  menu->AddGoToPage(0, "[Main]", 0, 0);
  cmi = menu->AddMultiChoice(onOffChoices, 2, 0, 1, menu_BackLightChanged, true);
  cmi->SetPrefix("Backlight   :");

  vmi = menu->AddValue(r17Resistance, 5, 1, 0, 2, menu_R17Changed);
  vmi->SetPrefix("R17 Value   :");
  vmi->SetSuffix("m\xF4");
  cmi = menu->AddMultiChoice(battTypes, 2, 0, 3, menu_BattTypeChanged, true);
  cmi->SetPrefix("Battery Type:");
  vmi = menu->AddValue(battCellCutOut, 4, 1, 0, 4, menu_cutOffChanged);
  vmi->SetPrefix("Batt Cut Off:");
  vmi->DigitIndex = 1;
  vmi = menu->AddValue(battCellCount, 1, 0, 0, 5, menu_battCellCountChanged);
  vmi->SetPrefix("Cell Count  :");
  menu->AddGoToPage(2, "[Fan & Temp]", 0, 6);

  //Page 2: Fan Management
  menu->AddPage();
  menu->AddGoToPage(1, "[Settings]", 0, 0);
  vmi = menu->AddValue(fanOnTemp[0], 4, 1, 0, 1, menu_FanOnTemp1Changed);
  vmi->SetPrefix("Level 1   :");
  vmi->DigitIndex = 1;
  vmi = menu->AddValue(fanOnTemp[1], 4, 1, 0, 2, menu_FanOnTemp2Changed);
  vmi->SetPrefix("Level 2   :");
  vmi->DigitIndex = 1;
  vmi = menu->AddValue(fanHysteresis, 3, 1, 0, 3, menu_FanHysteresisChanged);
  vmi->SetPrefix("Hysteresis:");
  vmi->DigitIndex = 1;

  menu->Configure(&lcd1, menu_pageChanged);
}

void timerIsr()
{
  encoder->service();
}

void setOutput()
{
  if (swLoadOnOff)
  {
    uint16_t newDacValue = round((double)setValues[workingMode] * (double)r17Resistance / 1000.0);
    dac.setValue(newDacValue);
    digitalWrite(P_LOADON_LED, HIGH);
    lcdRefreshMask |= UM_LOAD_ONOFF;
  }
  else
  {
    dac.setValue(0);
    digitalWrite(P_LOADON_LED, LOW);
    lcdRefreshMask |= UM_LOAD_ONOFF;
  }
}

void refreshDisplay()
{
  static Statistic drawStats;
  static unsigned long loopStart = 0;
  loopStart = micros();

  static char buffer[10];
  if (menu->GetCurrentPage() == 0)
  {
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
      Serial.println("Update Current on LCD");
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
    //Load On Off
    if (lcdRefreshMask & UM_LOAD_ONOFF)
    {
      lcd1.setCursor(0, 0);
      lcd1.print(swLoadOnOff ? "On :" : "Off:");
      lcdRefreshMask &= ~UM_LOAD_ONOFF;
      menu->PrintCursor();
    }
  }

  drawStats.add(micros() - loopStart);
  if (drawStats.count() == 5000)
  {
    Serial.print("RefeshLcd:Stats [Avg/Min->Max/Var]:");
    Serial.print((uint16_t)drawStats.average());
    Serial.print(" / ");
    Serial.print((uint16_t)drawStats.minimum());
    Serial.print("->");
    Serial.print((uint16_t)drawStats.maximum());
    Serial.print(" / ");
    Serial.println((uint16_t)drawStats.variance());
    drawStats.clear();
  }
}

void loadButton_click()
{
  Serial.println("Load CLick");
  swLoadOnOff = !swLoadOnOff;
  setOutput();
}

void loadButton_longClick()
{
}

void selfTest()
{
  uint8_t selfTestResult = 0;
  lcd1.home();
  lcd1.print(F("= Electronic  Load ="));

  //Check ADC Presence
  lcd1.setCursor(0, 1);
  lcd1.print("ADC:");
  Wire.requestFrom(ADC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x01;
  lcd1.print(selfTestResult & 0x01 ? "NOK" : "OK ");
  Serial.println(selfTestResult & 0x01 ? "ADC NOK" : "ADC OK");

  //Check DAC Presence
  lcd1.print("  DAC:");
  Wire.requestFrom(DAC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x02;
  lcd1.print(selfTestResult & 0x02 ? "NOK" : "OK");
  Serial.println(selfTestResult & 0x02 ? "DAC NOK" : "DAC OK");

  //Check RTC Presence
  lcd1.setCursor(0, 2);
  lcd1.print("RTC:");
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
  Wire.requestFrom(RTC_ADDR, (uint8_t)1);
  if (!Wire.available())
    selfTestResult |= 0x04;
  lcd1.print(selfTestResult & 0x04 ? "NOK" : "OK ");
  Serial.println(selfTestResult & 0x04 ? "RTC NOK" : "RTC OK");

  //Check IOX
  lcd1.print("  IOX:");
  Wire.beginTransmission(IOE_ADDR);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
  Wire.requestFrom(IOE_ADDR, (uint8_t)1);
  if (!Wire.available())
    selfTestResult |= 0x08;
  lcd1.print(selfTestResult & 0x08 ? "NOK" : "OK ");
  Serial.println(selfTestResult & 0x08 ? "IOE NOK" : "IOE OK");

  //Result ?
  if (selfTestResult > 0)
    delay(2000);

  //Booting
  Serial.println(F("Self test done"));
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
    {
      Serial.print("ADC-Ch1 Conv Err:");
      Serial.println(adcErr);
    }
    runConvertCh = -1;
  }
  if (runConvertCh == 2)
  {
    adcErr = adc.convert(ch2Config);
    if (adcErr)
    {
      Serial.print("ADC-Ch2 Conv Err:");
      Serial.println(adcErr);
    }
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
        Serial.print("ADC-Ch1 Read Err:");
        Serial.println(adcErr);
      }
      else
      {
        if (adcValue < 0)
          adcValue = 0;
        newVoltage = adcValue * (2048.0 / 32768) * 50;
        if (newVoltage != readVoltage)
        {
          readVoltage = newVoltage;
          Serial.print("New Voltage:");
          Serial.println(newVoltage);
          lcdRefreshMask |= (UM_VOLTAGE | UM_POWER);
        }
        runConvertCh = 2;
      }
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
        Serial.print("ADC-Ch2 Read Err:");
        Serial.println(adcErr);
      }
      else
      {
        if (adcValue < 0)
          adcValue = 0;
        newCurrent = adcValue * (2048.0 / 32768) * 2.5 * (1000.0 / r17Resistance);
        if (newCurrent != readCurrent)
        {
          readCurrent = newCurrent;
          Serial.print("New Current:");
          Serial.println(newCurrent);
          lcdRefreshMask |= (UM_CURRENT | UM_POWER);
        }
        runConvertCh = 1;
      }
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
    if (readTemperature > fanOnTemp[0] + fanHysteresis)
      fanLevel = 1;
    break;
  case 1:
    if (readTemperature < fanOnTemp[0] - fanHysteresis)
      fanLevel = 0;
    else if (readTemperature > fanOnTemp[1] + fanHysteresis)
      fanLevel = 2;
    break;
  case 2:
    if (readTemperature < fanOnTemp[1] - fanHysteresis)
      fanLevel = 1;
    break;
  }
  analogWrite(P_FAN, fanLevelPwm[fanLevel]);
}

Statistic loopStats;
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
  dac.setValue(1);
  dac.setValue(0);
  setOutput();

  _rtc.startClock();
  _rtc.setSQW(RTCx::freq32768Hz);

  Wire.setClock(400000); //Done after dac.Begin as it itself setClock to 100.000

  setupMenu();

  loopStats.clear();
}

void loop()
{
  static unsigned long loopStart = 0;
  static uint8_t buttonState;
  static uint8_t oldState = ClickEncoder::Open;

  loopStart = micros();
  pushButton->tick();
  actuateReadings();
  adjustFanSpeed();
  refreshDisplay();

  struct RTCx::tm tm;
  static unsigned long lastRtcRead = 0;
  if (millis() - lastRtcRead > 500)
  {
    _rtc.readClock(tm);
    RTCx::time_t t = RTCx::mktime(&tm);
    //Serial.println(t);
    lastRtcRead = millis();
  }

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

  loopStats.add(micros() - loopStart);
  if (loopStats.count() == 5000)
  {
    Serial.print("LoopStats:Stats [Avg/Min->Max/Var]:");
    Serial.print((uint16_t)loopStats.average());
    Serial.print(" / ");
    Serial.print((uint16_t)loopStats.minimum());
    Serial.print("->");
    Serial.print((uint16_t)loopStats.maximum());
    Serial.print(" / ");
    Serial.println((uint16_t)loopStats.variance());
    loopStats.clear();
  }
}