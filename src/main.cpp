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

//BEGIN - PIN ATTRIBUTION ###
#ifdef DEBUG_BOARD
#define P_ENC1 4
#define P_ENC2 3
#define P_ENCBTN 5
#define P_LOADON_BTN 8
#define P_LOADON_LED 7
#define P_LM35 A6
#define P_FAN 7
#else
#define P_ENC1 4
#define P_ENC2 3
#define P_ENCBTN 7
#define P_LOADON_BTN 6
#define P_LOADON_LED 9
#define P_LM35 A6
#define P_FAN 5
#endif
//END - PIN ATTRIBUTION ###

//BEGIN ADC-DAC
#ifdef DEBUG_BOARD
#define DAC_ADDR 0x60
#else
#define DAC_ADDR 0x61
#endif
#define ADC_ADDR 0x68
#define DAC_REF_VOLTAGE 3300

MCP4725 dac(DAC_ADDR);
MCP342x adc = MCP342x(ADC_ADDR);
//END ADC-DAC

//BEGIN - LCD ###
LiquidCrystal_PCF8574 lcd1(0x27);
int backlight = 0;
#define UM_TEMP 1
#define UM_VOLTAGE 2
#define UM_CURRENT 4
uint8_t lcd_refresh_mask = 255;
#define WORKINGMODE_COUNT 4
const char *working_modes[WORKINGMODE_COUNT] = {"CCur", "CRes", "CPow", "Batt"};
const char *onOffChoices[2] = {"On", "Off"};
const char mode_dimensions[5] = {'I', 'V', 'R', 'P', 'I'};
const char mode_units[5] = {'A', 'V', (char)0xF4, 'W', 'A'};
Menu *menu;
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
uint8_t workingMode = 0;
int16_t setValues[WORKINGMODE_COUNT] = {0, 0, 0, 0};
uint16_t read_temp = 0; // 1/10 °C
uint16_t fanOnTemp[] = {300, 400};
uint8_t fanHysteresis = 20;
int16_t read_current = 0; // mA
int16_t read_voltage = 0; // mV
bool onOff = false;
//END - Load ###

OneButton *pushButton;

void timerIsr();
void printSettingsDimensionAndUnits();
bool menu_modeChanged(int32_t newMode);
bool menu_setValueChanged(int32_t newValue);
void menu_pageChanged(uint8_t newPageIndex);
void setupMenu();
void setOut();
void refreshDisplay();
void loadButton_click();
void loadButton_longClick();
void selfTest();
void setupLoadButton();
void setupLoadButton();

void timerIsr()
{
  encoder->service();
}

void printSettingsDimensionAndUnits()
{
  lcd1.setCursor(4, 2);
  lcd1.write(mode_dimensions[workingMode]);
  lcd1.write(':');
  lcd1.setCursor(12, 2);
  lcd1.write(mode_units[workingMode]);
}

bool menu_modeChanged(int8_t newMode)
{
  workingMode = newMode;
  printSettingsDimensionAndUnits();
  return true;
}

bool menu_setValueChanged(int32_t newValue)
{
  if (newValue < 0)
    return false;
  if (newValue > 30000)
    return false;
  setValues[workingMode] = newValue;
  setOut();
  return true;
}

void menu_pageChanged(uint8_t newPageIndex)
{
  lcd1.clear();
  switch (newPageIndex)
  {
  case 0:
    lcd1.setCursor(0, 0);
    lcd1.print("Mode:");

    lcd1.setCursor(15, 0);
    lcd1.print((char)0xDF);
    lcd1.print("C");

    lcd1.setCursor(5, 1);
    lcd1.print("A");
    lcd1.setCursor(12, 1);
    lcd1.print("V");
    lcd1.setCursor(19, 1);
    lcd1.print("W");

    lcd1.setCursor(0, 2);
    lcd1.print("SET ");
    printSettingsDimensionAndUnits();
    lcd_refresh_mask = 0xFF;
    break;
  case 1:
    lcd1.setCursor(0, 1);
    lcd1.print("BackLight:");
    break;
  case 2:
    lcd1.setCursor(0, 1);
    lcd1.print("On Temp  :");
    lcd1.setCursor(0, 2);
    lcd1.print("Hysteres :");
    lcd1.setCursor(0, 3);
    lcd1.print("Fan FdBck:");
    break;
  }
}

bool menu_BackLightChanged(int8_t newValue)
{
  Serial.print("menu_BackLightChanged");
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

void setupMenu()
{
  menu = new Menu(3, 10);
  //Page 0: Main Page
  menu->AddPage();
  menu->AddMultiChoice(working_modes, WORKINGMODE_COUNT, 5, 0, menu_modeChanged, false);
  menu->AddValue(0, 6, 3, 6, 2, menu_setValueChanged);
  menu->AddGoToPage(1, "Settings", 0, 3);
  //Page 1: Settings
  menu->AddPage();
  menu->AddGoToPage(0, "[Main]", 0, 0);
  menu->AddMultiChoice(onOffChoices, 2, 10, 1, menu_BackLightChanged, true);
  menu->AddGoToPage(2, "Fan & Temp", 0, 2);
  //Page 2: Fan Management
  menu->AddPage();
  menu->AddGoToPage(1, "[Settings]", 0, 0);
  MultiDigitValueMenuItem *vmi;
  vmi = menu->AddValue(fanOnTemp[0], 4, 1, 10, 1, menu_FanOnTemp1Changed);
  vmi->DigitIndex = 1;
  vmi = menu->AddValue(fanOnTemp[1], 4, 1, 10, 2, menu_FanOnTemp2Changed);
  vmi->DigitIndex = 1;
  vmi = menu->AddValue(fanHysteresis, 3, 1, 10, 3, menu_FanHysteresisChanged);
  vmi->DigitIndex = 1;
  //menu->AddMultiChoice(onOffChoices, 2, 10, 4, menu_FanFeedbackChanged, true);

  menu->Configure(&lcd1, menu_pageChanged);
}

void setOut()
{
  if (onOff)
  {
    uint16_t newDacValue = ((double)setValues[workingMode] / 1000) * (4096.0 / 3.3);
    dac.setValue(newDacValue);
    digitalWrite(P_LOADON_LED, HIGH);
  }
  else
  {
    dac.setValue(0);
    digitalWrite(P_LOADON_LED, LOW);
  }
}

void refreshDisplay()
{
  static char buffer[10];
  if (menu->GetCurrentPage() == 0)
  {
    //Temp
    if (lcd_refresh_mask & UM_TEMP)
    {
      lcd1.setCursor(11, 0);
      lcd1.print(dtostrf((double)read_temp / 10, 3, 1, buffer));
      lcd1.setCursor(17, 0);
      lcd1.print("[");
      lcd1.print(fanLevel);
      lcd1.print("]");
    }

    //Readings
    if (lcd_refresh_mask && UM_CURRENT)
    {
      lcd1.setCursor(0, 1);
      lcd1.print(dtostrf((double)read_current / 1000, 5, 3, buffer));
    }
    if (lcd_refresh_mask && UM_VOLTAGE)
    {
      lcd1.setCursor(7, 1);
      lcd1.print(dtostrf((double)read_voltage / 1000, 5, 3, buffer));
    }
    if (lcd_refresh_mask & (UM_CURRENT | UM_VOLTAGE))
    {
      lcd1.setCursor(14, 1);
      lcd1.print(dtostrf((double)read_voltage * read_current / 1000000, 5, 3, buffer));
    }
  }
  if (lcd_refresh_mask > 0)
    menu->PrintCursor();
  lcd_refresh_mask = 0;
}

void loadButton_click()
{
  Serial.println("Load CLick");
  onOff = !onOff;
  setOut();
}

void loadButton_longClick()
{
}

void selfTest()
{
  uint8_t selfTestResult = 0;
  lcd1.home();
  lcd1.print("= Electronic  Load =");

  //Check ADC Presence
  lcd1.setCursor(0, 1);
  lcd1.print("ADC on ");
  lcd1.print(ADC_ADDR, HEX);
  lcd1.print(":");
  Wire.requestFrom(ADC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x01;
  lcd1.print(Wire.available() ? "OK" : "NOK");
  Serial.println(Wire.available() ? "ADC OK" : "ADC NOK");

  //Check DAC Presence
  lcd1.setCursor(0, 2);
  lcd1.print("DAC on ");
  lcd1.print(DAC_ADDR, HEX);
  lcd1.print(":");
  Wire.requestFrom(DAC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x02;
  lcd1.print(Wire.available() ? "OK" : "NOK");
  Serial.println(Wire.available() ? "DAC OK" : "DAC NOK");

  //Check RTC Presence
  lcd1.setCursor(0, 3);
  lcd1.print("RTC ");

  if (rtc.autoprobe())
  {
    lcd1.print(rtc.getDeviceName());
    lcd1.print(" at ");
    lcd1.print(rtc.getAddress());
  }
  else
  {
    lcd1.print("Not found");
    selfTestResult |= 0x04;
  }

  //Result ?
  lcd1.setCursor(0, 4);
  if (selfTestResult > 0)
  {
    lcd1.print("Failed :");
    lcd1.print(selfTestResult, HEX);
    delay(2000);
  }

  //Booting
  lcd1.print("Booting...");
  delay(500);
  Serial.println("Self test done");
}

//TODO : Auto Gain adjustment : For low voltage, 4x will increase resolution
SMA<10, uint16_t, uint32_t> fanSma;
void performReadings()
{
  static MCP342x::Config ch1Config(MCP342x::channel1, MCP342x::oneShot, MCP342x::resolution16, MCP342x::gain1);
  static MCP342x::Config ch2Config(MCP342x::channel2, MCP342x::oneShot, MCP342x::resolution16, MCP342x::gain1);
  static MCP342x::Config adcStatus;
  static long adcValue;
  static uint8_t adcErr;
  static int8_t runConvertCh = 1;

  static uint8_t rawTemp;
  static unsigned long lastTempRefreshTime = 0;

  rawTemp = fanSma(analogRead(P_LM35));
  read_temp = rawTemp * 4.83871;
  if (lastTempRefreshTime + 1000 < millis())
  {
    lcd_refresh_mask |= UM_TEMP;
    lastTempRefreshTime = millis();
  }

  if (runConvertCh == 1)
  {
    adcErr = adc.convert(ch1Config);
    if (adcErr)
    {
      Serial.print("ADC-Ch1 Err:");
      Serial.println(adcErr);
      delay(1000);
    }
    runConvertCh = -1;
  }
  if (runConvertCh == 2)
  {
    adcErr = adc.convert(ch2Config);
    if (adcErr)
    {
      Serial.print("ADC-Ch2 Err:");
      Serial.println(adcErr);
      delay(1000);
    }
    runConvertCh = -2;
  }

  if (runConvertCh == -1)
  {
    static int16_t newVoltage = 0;
    adcErr = adc.read(adcValue, adcStatus);
    if (!adcErr && adcStatus.isReady())
    {
      newVoltage = adcValue * 50 * (2048.0 / 32768);
      if (read_voltage != newVoltage)
      {
        read_voltage = newVoltage;
        lcd_refresh_mask |= UM_VOLTAGE;
      }
      runConvertCh = 2;
    }
  }

  if (runConvertCh == -2)
  {
    static int16_t newCurrent = 0;
    adcErr = adc.read(adcValue, adcStatus);
    if (!adcErr && adcStatus.isReady())
    {
      newCurrent = adcValue * (2048.0 / 32768);
      if (read_current != newCurrent)
      {
        read_current = newCurrent;
        lcd_refresh_mask |= UM_CURRENT;
      }
      runConvertCh = 1;
    }
  }
}

//TODO Adjust PWM's from settings (need scrollable menu)
uint8_t fanLevel = 0;
void adjustFanSpeed()
{
  static uint16_t fanLevelPwm[] = {0, 900, 1023};
  switch (fanLevel)
  {
  case 0:
    if (read_temp > fanOnTemp[0] + fanHysteresis)
      fanLevel = 1;
    break;
  case 1:
    if (read_temp < fanOnTemp[0] - fanHysteresis)
      fanLevel = 0;
    else if (read_temp > fanOnTemp[1] + fanHysteresis)
      fanLevel = 2;
    break;
  case 2:
    if (read_temp < fanOnTemp[1] - fanHysteresis)
      fanLevel = 1;
    break;
  }
  analogWrite(P_FAN, fanLevelPwm[fanLevel]);
}

Statistic loopStats;
void setup()
{
  //Wire.setClock(400000);
  Serial.begin(9600);
  Serial.print("Hello");

  // initialize the lcd
  lcd1.begin(20, 4);
  lcd1.setBacklight(255);

  pinMode(P_LM35, INPUT);
  pinMode(P_FAN, OUTPUT);
  digitalWrite(P_FAN, LOW);
  pinMode(P_LOADON_LED, OUTPUT);

  //Setup Encoder
  encoder = new ClickEncoder(P_ENC1, P_ENC2, P_ENCBTN, ENC_STEPS);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  encoder->setAccelerationEnabled(true);

  //Setup Load button
  pushButton = new OneButton(P_LOADON_BTN, 0, false);
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
  setOut();

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
  performReadings();
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