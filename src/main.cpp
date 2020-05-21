#define DEBUG_BOARD false
#define EDCL_DEBUG false
#define DEBUG_TIMINGS EDCL_DEBUG && false

#include <Wire.h>
#include <Arduino.h>
#include <ClickEncoder.h>
#include <OneButton.h>
#include <TimerOne.h>
#include <LiquidCrystal_PCF8574.h>
#include <MCP4725.h>
#include <MCP342x.h>
#include <Menu.h>
#include <Filters/SMA.hpp>
#include <SPIFlash.h>
#include <MemoryFree.h>
#include <Settings.h>
#include <debug.h>
#include <MCP79410_Timer.h>
#include "header.h"
#include <specialLcdChar.h>
#include "menumg.h"
#include <Distribution.h>

//BEGIN ADC-DAC
#if DEBUG_BOARD
#define DAC_ADDR 0x60
#define ENC_STEPS 2
#else
#define DAC_ADDR 0x61
#define ENC_STEPS 4
#endif
#define ADC_ADDR 0x68
#define RTC_ADDR 0x6F
#define IOE_ADDR 0x20
#define DAC_REF_VOLTAGE 3300

MCP4725 dac(DAC_ADDR);
MCP342x adc = MCP342x(ADC_ADDR);
MCP79410_Timer _rtcTimer(RTC_ADDR);
SPIFlash flash(P_FLASH_SS, 0xEF30);
LiquidCrystal_PCF8574 lcd1(0x27);

ClickEncoder *encoder;
OneButton *pushButton;

extern Menu *menu;
const char *workingModes[WORKINGMODE_COUNT] = {"CC", "CR", "CP", "Ba"};
const char *battTypes[BATT_TYPE_COUNT] = {"LiPo", "NiMh", "LiFe", "Pb"};
const char *triggerType[TRIGGER_TYPE_COUNT] = {"Man.", "Flip", "Timr"};
struct Settings *settings = new Settings();
uint8_t readTemperature = 0; // 1/10 °C
int16_t readCurrent = 0;     // mA
int16_t readVoltage = 0;     // mV
double totalmAh = 0;
uint8_t lcdRefreshMask = 0;

uint8_t state = 0;
#define fanLevelState ((state >> 1) & 0x03)
#define setFanLevel(l) state = (state & ~(0x03 << 1)) | ((l & 0x03) << 1)

void timerIsr()
{
  encoder->service();
}

void LoadOn()
{
  state |= STATE_ONOFF;
  digitalWrite(P_LOADON_LED, HIGH);
  lcdRefreshMask |= UM_LOAD_ONOFF;
  setOutput();
  _rtcTimer.start();
}

void LoadOff()
{
  state &= ~STATE_ONOFF;
  digitalWrite(P_LOADON_LED, LOW);
  lcdRefreshMask |= UM_LOAD_ONOFF;
  setOutput();
  _rtcTimer.stop();
}

void setOutput()
{
  if (state & STATE_ONOFF)
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
  lcdRefreshMask &= (UM_TEMP | UM_VOLTAGE | UM_CURRENT | UM_TIME | UM_LOAD_ONOFF | UM_ALERT);
  if (lcdRefreshMask == 0 || menu->GetCurrentPage() != 0)
    return;
  static char buffer[10];

  //Load On Off
  if (lcdRefreshMask & UM_LOAD_ONOFF)
  {
    lcd1.setCursor(0, 0);
    lcd1.print(state & STATE_ONOFF ? F("On :") : F("Off:"));
    lcdRefreshMask &= ~UM_LOAD_ONOFF;
  }

  //Temp
  if (lcdRefreshMask & UM_TEMP)
  {
    lcd1.setCursor(16, 0);
    lcd1.print(readTemperature);
    lcd1.setCursor(19, 0);
    lcd1.write(SC_THERMO_L0 + fanLevelState);
    lcdRefreshMask &= ~UM_TEMP;
  }

  //Readings
  if (lcdRefreshMask & (UM_CURRENT | UM_VOLTAGE))
  {
    if (lcdRefreshMask & UM_CURRENT)
    {
      lcd1.setCursor(0, 1);
      lcd1.print(dtostrf((double)readCurrent / 1000, 5, 3, buffer));
      lcdRefreshMask &= ~UM_CURRENT;
    }
    if (lcdRefreshMask & UM_VOLTAGE)
    {
      lcd1.setCursor(7, 1);
      lcd1.print(dtostrf((double)readVoltage / 1000, 6, 3, buffer));
      lcdRefreshMask &= ~UM_VOLTAGE;
    }
    lcd1.setCursor(15, 1);
    lcd1.print(dtostrf((double)readVoltage * readCurrent / 1000000, 4, 1, buffer));
  }

  //Time
  static unsigned long lastTotalSec = 0;
  static unsigned long lastTmUpdate = 0;
  if ((lcdRefreshMask & UM_TIME) || (lastTmUpdate + 200 < millis() && lastTotalSec != _rtcTimer.getTotalSeconds()))
  {
    lcd1.setCursor(1, 3);
    _rtcTimer.getTime(buffer);
    lcd1.print(buffer);

    lastTotalSec = _rtcTimer.getTotalSeconds();
    lastTmUpdate = millis();

    lcd1.setCursor(10, 3);
    dtostrf(totalmAh / 3600000, 7, 1, buffer);
    lcd1.print(buffer);

    lcdRefreshMask &= ~UM_TIME;
  }

  if (lcdRefreshMask & UM_ALERT)
  {
    lcd1.setCursor(9, 0);
    lcd1.print(state & STATE_SUSPICIOUS_CELL_COUNT ? "!" : " ");
    lcdRefreshMask &= ~UM_ALERT;
  }

  //Cursor
  menu->PrintCursor();
}

void loadButton_click()
{
  if (state & STATE_ONOFF)
    LoadOff();
  else
    LoadOn();
  SaveSettings();
}

void loadButton_longClick()
{
  if (!(state & STATE_ONOFF))
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

void DetectSuspiciousCellCount()
{
  int16_t min = settings->battCellCount * battMinVoltage[settings->battType] * 100;
  int16_t max = settings->battCellCount * battMaxVoltage[settings->battType] * 100;
  bool suspicious = readVoltage < min || readVoltage > max;
  if (suspicious != (state & STATE_SUSPICIOUS_CELL_COUNT))
  {
    lcdRefreshMask |= UM_ALERT;
    state = suspicious ? state | STATE_SUSPICIOUS_CELL_COUNT : state & ~STATE_SUSPICIOUS_CELL_COUNT;
  }
}

//TODO : Auto Gain adjustment : For low voltage, 4x will increase resolution
//TODO : Manage error conditions
enum adcStage
{
  runConvertCh1,
  runConvertCh2,
  readCh1,
  readCh2
};
SMA<10, uint8_t, uint16_t> temperatureSMA;
void actuateReadings()
{
  static enum adcStage readingStage = runConvertCh1;

  MCP342x::Config adcStatus;
  long adcValue;

  //Temperature Reading
  static unsigned long lastTempUpdate = 0;
  if (lastTempUpdate + LCD_TEMP_MAX_UPDATE_RATE < millis())
  {
    uint8_t newTemperature = temperatureSMA(analogRead(P_LM35)) * 0.32258; // 3.3V / 1023 * 100°C/V * 10
    if (newTemperature != readTemperature)
    {
      readTemperature = newTemperature;
      lcdRefreshMask |= UM_TEMP;
    }
    lastTempUpdate = millis();
  }

  //Voltage & Current Reading (alternate)
  int16_t newCurrent = 0;
  int16_t newVoltage = 0;
  static unsigned long lastCurrentUpdate = 0;
  static unsigned long lastVoltageUpdate = 0;
  switch (readingStage)
  {
  case runConvertCh1:
    adc.convert(MCP342x::channel1, MCP342x::oneShot, MCP342x::resolution16, MCP342x::gain1);
    readingStage = readCh1;
    break;
  case runConvertCh2:
    adc.convert(MCP342x::channel2, MCP342x::oneShot, MCP342x::resolution16, MCP342x::gain4);
    readingStage = readCh2;
    break;
  case readCh1:
    adc.read(adcValue, adcStatus);
    if (adcStatus.isReady())
    {
      if (adcValue < 0)
        adcValue = 0;
      newVoltage = adcValue * (2048.0 / 32768) * 50;
      if (newVoltage != readVoltage || lastVoltageUpdate + 500 < millis())
      {
        readVoltage = newVoltage;
        if (settings->mode == MODE_BA)
          DetectSuspiciousCellCount();
        lcdRefreshMask |= UM_VOLTAGE;
        lastVoltageUpdate = millis();
      }
      readingStage = runConvertCh2;
    }
    break;
  case readCh2:
    adc.read(adcValue, adcStatus);
    if (adcStatus.isReady())
    {
      if (adcValue < 0)
        adcValue = 0;
      newCurrent = adcValue * (2048.0 / 32768) * 2.5 * (1000.0 / settings->r17Value);
      if (newCurrent != readCurrent || lastCurrentUpdate + 500 < millis())
      {
        if (state & STATE_ONOFF)
          totalmAh += readCurrent * (millis() - lastCurrentUpdate);
        readCurrent = newCurrent;
        lcdRefreshMask |= UM_CURRENT;
        lastCurrentUpdate = millis();
      }
      readingStage = runConvertCh1;
    }
    break;
  }
}

//TODO Adjust PWM's from settings (need scrollable menu)
void adjustFanSpeed()
{
  static uint16_t fanLevelPwm[] = {0, 900, 1023};
  switch (fanLevelState)
  {
  case 0:
    if (readTemperature > settings->fanTemps[0] + settings->fanHysteresis)
      setFanLevel(1);
    break;
  case 1:
    if (readTemperature < settings->fanTemps[0] - settings->fanHysteresis)
      setFanLevel(0);
    else if (readTemperature > settings->fanTemps[1] + settings->fanHysteresis)
      setFanLevel(2);
    break;
  case 2:
    if (readTemperature < settings->fanTemps[1] - settings->fanHysteresis)
      setFanLevel(1);
    break;
  }
  analogWrite(P_FAN, fanLevelPwm[fanLevelState]);
}

void setup()
{
  Serial.begin(9600);
  // initialize the lcd
  lcd1.begin(20, 4);
  lcd1.setBacklight(255);
  setupSPecialLcdChars(&lcd1);

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
#if DEBUG_BOARD
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
  if (!ReadSettings())
  {
    //Enjoy this first booting time to setup DAC POR Value (1/2 ref voltage out of factory)
    dac.writeDAC(0, true);
    while (!dac.RDY())
      ;
  }
  SetBacklight();

  Wire.setClock(400000); //Done after dac.Begin as it itself setClock to 100.000

  setupMenu();
  menu_modeChanged(settings->mode);

  lcdRefreshMask = 0xFF;
}

void loop()
{
#if DEBUG_TIMINGS
  static Distribution ellapsedDistribution(20);
  static unsigned long loopStart = 0;
  loopStart = millis();
#endif
  static uint16_t loopCOunt = 0;
  static uint8_t buttonState;
  static uint8_t oldState = ClickEncoder::Open;

  pushButton->tick();
  actuateReadings();
  if (readTemperature > settings->fanTemps[2])
  {
    setFanLevel(3);
    LoadOff();
  }
  if (state & STATE_ONOFF && settings->mode == MODE_BA)
  {
    int16_t cutOffVoltage = settings->battCellCount * settings->battCutOff[settings->battType];
    if (readVoltage < cutOffVoltage)
      LoadOff();
  }

  adjustFanSpeed();
  setOutput();
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

  if (loopCOunt++ >= 10000)
  {
    Serial.print("Free:");
    Serial.println(freeMemory());
    loopCOunt = 0;
  }

#if DEBUG_TIMINGS
  ellapsedDistribution.Add(millis() - loopStart);
  if (ellapsedDistribution.samples >= 5000)
  {
    Serial.print("Loop Tm Distri :");
    ellapsedDistribution.Print();
    ellapsedDistribution.Clear();
  }
#endif
}