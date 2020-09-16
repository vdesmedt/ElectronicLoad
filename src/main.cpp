#define DEBUG_BOARD false
#define EDCL_DEBUG true
#define DEBUG_TIMINGS EDCL_DEBUG && false
#define DEBUG_MEMORY EDCL_DEBUG && true

#include "header.h"

const char *onOffChoices[] = {"On ", "Off"};
const char *modeUnits[WORKINGMODE_COUNT] = {"A", "\xF4", "W", "A"};
const char *workingModes[WORKINGMODE_COUNT] = {"CC", "CR", "CP", "Ba"};
const char *battTypes[BATT_TYPE_COUNT] = {"LiPo", "NiMh", "LiFe", "Pb"};
const char *triggerType[TRIGGER_TYPE_COUNT] = {"Off ", "Flip", "Timr"};
const char *loggingMode[LOGGIN_MODE_COUNT] = {"Off  ", "St Ser ", "Bi Ser", "RFM69"};

const int8_t battMinVoltage[BATT_TYPE_COUNT] = {30, 8, 25, 18};  // 1/10th Volt
const int8_t battMaxVoltage[BATT_TYPE_COUNT] = {42, 15, 36, 23}; // 1/10th Volt

Menu *menu;
struct Settings *settings = new Settings();
uint16_t readTemperature = 0; // 1/10 °C
int16_t readCurrent = 0;      // mA
int16_t readVoltage = 0;      // mV
double totalmAh = 0;
uint8_t lcdRefreshMask = 0;
unsigned long iddleSince = 0;

uint8_t state = 0;
#define fanLevelState ((state >> 1) & 0x03)
#define setFanLevel(l) state = (state & ~(0x03 << 1)) | ((l & 0x03) << 1)

MCP4725 dac(DAC_ADDR);
MCP342x adc = MCP342x(ADC_ADDR);
MCP79410_Timer _rtcTimer(RTC_ADDR);
SPIFlash flash(P_FLASH_SS, 0xEF30);
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI mainLcd(U8G2_R0, /* cs=*/3, /* dc=*/1, /* reset=*/0);
ClickEncoder *encoder;
OneButton *pushButton;

void timerIsr()
{
  encoder->service();
}

void LoadOn()
{
  state |= STATE_ONOFF;
  digitalWrite(P_LOADON_LED, LOW);
  lcdRefreshMask |= UM_LOAD_ONOFF;
  setOutput();
  _rtcTimer.start();
}

void LoadOff()
{
  state &= ~STATE_ONOFF;
  digitalWrite(P_LOADON_LED, HIGH);
  lcdRefreshMask |= UM_LOAD_ONOFF;
  setOutput();
  _rtcTimer.stop();
}

bool getTriggerState()
{
  if (settings->triggerType == TRIGGER_OFF)
    return true;

  static int oldTriggerState = HIGH;
  static unsigned long lastTriggerTimeMs = 0;
  static bool flipState = false;
  int triggerState = digitalRead(P_TRIGGER);
  if (triggerState != oldTriggerState && triggerState == LOW)
  {
    debug_print(F("Trigger detected\n"));
    switch (settings->triggerType)
    {
    case TRIGGER_FLIP:
      flipState = !flipState;
      break;
    case TRIGGER_TIMER:
      lastTriggerTimeMs = millis();
      break;
    }
  }
  oldTriggerState = triggerState;
  switch (settings->triggerType)
  {
  case TRIGGER_FLIP:
    return flipState;
  case TRIGGER_TIMER:
    return lastTriggerTimeMs + settings->triggerTimer > millis();
  }

  return false;
}

void setOutput()
{
  static double previousDacValue = -1;
  if ((state & STATE_ONOFF) && getTriggerState())
  {
    double newDacValue = 0; //Output in mV -> mA
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
    newDacValue *= (double)settings->r17Value / 1000.0;
    if (previousDacValue != newDacValue)
    {
      debug_printb(F("New DAC Value:"), "%d\n", (int)round(newDacValue));
      dac.setValue(round(newDacValue));
      previousDacValue = newDacValue;
    }
  }
  else
  {
    dac.setValue(0);
    previousDacValue = -1;
  }
}

#include <MMDigit12.h>
#include <MMDigit08.h>

void refreshDisplay()
{
  static unsigned long lastRefresh = millis();
  if ((lcdRefreshMask | UM_ALL) == 0 && millis() < lastRefresh + 500)
    return;
  static char buffer[10];
  mainLcd.clearBuffer();
  if (menu->GetCurrentPage() == 0)
  {
    mainLcd.setFont(MMDigit12);
    //Voltage
    mainLcd.drawStr(0, 30, dtostrf((double)readVoltage / 1000, 6, 3, buffer));
    if (readVoltage / 1000 < 10)
      mainLcd.drawStr(0, 30, "0");
    //Current
    mainLcd.drawStr(0, 45, dtostrf((double)readCurrent / 1000, 6, 3, buffer));

    mainLcd.setFont(MMDigit08);
    //Temp
    snprintf(buffer, 10, "%d", readTemperature / 10);
    mainLcd.drawStr(104, 26, buffer);
    //Power
    mainLcd.drawStr(107, 36, dtostrf((double)readVoltage * readCurrent / 1000000, -4, 1, buffer));
    //Time & mAh
    _rtcTimer.getTime(buffer);
    mainLcd.drawStr(89, 46, buffer);

    dtostrf(totalmAh / 3600000, 7, 1, buffer);
    mainLcd.drawStr(59, 57, buffer);

    mainLcd.setFont(u8g2_font_6x10_tf);
    //OnOff
    mainLcd.setFont(u8g2_font_6x10_tf);
    mainLcd.drawStr(0, 13, state & STATE_ONOFF ? "On" : "Off");

    //Units
    mainLcd.drawStr(40, 28, "V");
    mainLcd.drawStr(40, 44, "A");
    mainLcd.drawStr(123, 35, "W");
    mainLcd.drawStr(117, 25, "\xb0\x43"); //°C
    mainLcd.drawStr(111, 56, "mAh");
  }

  menu->Print();
  mainLcd.sendBuffer();
  lcdRefreshMask = UM_NONE;
}

void loadButton_click()
{
  iddleSince = millis();
  if (state & STATE_ONOFF)
    LoadOff();
  else
    LoadOn();
}

void loadButton_longClick()
{
  iddleSince = millis();
  if (!(state & STATE_ONOFF))
  {
    _rtcTimer.reset();
    totalmAh = 0;
  }
}

void selfTest()
{
  mainLcd.clearBuffer();
  uint8_t selfTestResult = 0;
  mainLcd.drawStr(10, 8, "= Electronic  Load =");
  mainLcd.drawStr(0, 20, "ADC DAC RTC IOE FLH");

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
  Wire.requestFrom(RTC_ADDR, 1);
  if (!Wire.available())
    selfTestResult |= 0x04;

  //Check IOX
  Wire.beginTransmission(IOE_ADDR);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
  Wire.requestFrom(IOE_ADDR, 1);
  if (!Wire.available())
    selfTestResult |= 0x08;

  //Check Flash
  if (!flash.initialize())
    selfTestResult |= 0x10;

  //Result ?
  for (uint8_t i = 0; i < 5; i++)
  {
    mainLcd.drawStr(0 + 10 * i, 30, selfTestResult & (1 << i) ? "X" : "V");
  }
  mainLcd.sendBuffer();

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

    uint16_t newTemperature = temperatureSMA(analogRead(P_LM35)) * 3.376; //1023/3.3 Step/V, 100°C/V
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
      //newVoltage = adcValue * (2048.0 / 32768.0) * 50;
      newVoltage = (float)adcValue * 3.125;
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
      //newCurrent = adcValue * (2048.0 / 32768.0) * 2.5 * (1000.0 / (float)settings->r17Value);
      newCurrent = (float)adcValue * 156.25 / (float)settings->r17Value;
      if (newCurrent != readCurrent || lastCurrentUpdate + 500 < millis())
      {
        if (state & STATE_ONOFF)
          totalmAh += readCurrent * (millis() - lastCurrentUpdate);
        lastCurrentUpdate = millis();
        readCurrent = newCurrent;
        lcdRefreshMask |= UM_CURRENT;
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

void LogData()
{
  switch (settings->loggingType)
  {
  case LOG_SERIAL:
    Serial.print(_rtcTimer.getTotalSeconds());
    Serial.print(",");
    Serial.print(readVoltage);
    Serial.print(",");
    Serial.print(readCurrent);
    Serial.print(",");
    Serial.print(readTemperature);
    Serial.print(",");
    Serial.println(totalmAh / 3600000);
    break;
  case LOG_PACKET_SERIAL:
    break;
  case LOG_RFM69:
    break;
  }
}

void setup()
{
  Serial.begin(9600);
  // initialize the lcd
  mainLcd.begin();
  mainLcd.clearBuffer();

  pinMode(P_LM35, INPUT);
  pinMode(P_FAN, OUTPUT);
  digitalWrite(P_FAN, LOW);
  pinMode(P_LOADON_LED, OUTPUT);
  pinMode(P_TRIGGER, INPUT_PULLUP);

  //Setup Encoder
  encoder = new ClickEncoder(P_ENC1, P_ENC2, P_ENCBTN, ENC_STEPS);
  Timer3.initialize(1000);
  Timer3.attachInterrupt(timerIsr);
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

  Wire.setClock(400000); //Done after dac.Begin as it itself setClock to 100.000

  setupMenu();
  menu_modeChanged(settings->mode);
  settings->version &= ~1; //Reset dirty flag modified by menu_modeChanged
  lcdRefreshMask = UM_ALL;
}

void loop()
{
#if DEBUG_TIMINGS
  static Distribution ellapsedDistribution(20);
  static unsigned long loopStart = 0;
  loopStart = millis();
#endif
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
  static unsigned long lastLogTime = 0;
  if (state & STATE_ONOFF && settings->loggingType != 0 && lastLogTime + (settings->loggingInterval * 1000) < millis())
  {
    LogData();
    lastLogTime = millis();
  }

  //Encoder
  int16_t enc = encoder->getValue();
  if (enc != 0)
  {
    iddleSince = millis();
    menu->EncoderInc(enc);
  }

  //Encoder Button
  buttonState = encoder->getButton();
  if (buttonState == ClickEncoder::Clicked)
  {
    iddleSince = millis();
    menu->Click();
  }
  else if (buttonState == ClickEncoder::Held && oldState != ClickEncoder::Held)
  {
    iddleSince = millis();
    menu->LongClick();
  }
  oldState = buttonState;

  if (!(state & STATE_ONOFF) && settings->version & 0x01 && iddleSince + 5000 < millis())
    SaveSettings();

#if DEBUG_MEMORY
  static uint16_t loopCount = 0;
  if (loopCount++ >= 10000)
  {
    debug_printb(F("Free:"), "%i\n", freeMemory());
    loopCount = 0;
  }
#endif

#if DEBUG_TIMINGS
  ellapsedDistribution.Add(millis() - loopStart);
  if (ellapsedDistribution.samples >= 10000)
  {
    Serial.print("Loop Tm Distri :");
    ellapsedDistribution.Print();
    ellapsedDistribution.Clear();
  }
#endif
}