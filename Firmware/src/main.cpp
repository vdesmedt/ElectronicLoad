#define DEBUG_BOARD false
#define EDCL_DEBUG true
#define DEBUG_TIMINGS EDCL_DEBUG && true
#define DEBUG_MEMORY EDCL_DEBUG && true

#include "header.h"

const char *onOffChoices[] = {"On ", "Off"};
const char *modeUnits[WORKINGMODE_COUNT] = {"A", "\xF4", "W", "A"};
const char *workingModes[WORKINGMODE_COUNT] = {"CCur", "CRes", "CPow", "Batt"};
const char *triggerType[TRIGGER_TYPE_COUNT] = {"Off", "Flip", "Timr"};
const char *loggingMode[LOGGIN_MODE_COUNT] = {"Off", "St Ser", "Bi Ser", "RFM69"};

const char *battTypes[BATT_TYPE_COUNT] = {"LiPo", "NiMh", "LiFe", "Pb"};
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
uint8_t alerts = 0;
#define fanLevelState ((state >> 1) & 0x03)
#define setFanLevel(l) state = (state & ~(0x03 << 1)) | ((l & 0x03) << 1)

MCP4725 dac(DAC_ADDR);
MCP342x adc = MCP342x(ADC_ADDR);
MCP79410_Timer _rtcTimer(RTC_ADDR);
SPIFlash flash(P_FLASH_SS, 0xEF30);
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI mainLcd(U8G2_R0, /* cs=*/3, /* dc=*/1, /* reset=*/0);
ClickEncoder *encoder;
OneButton *loadOnOffButton;
OneButton *vsenseButton;
HHLogger *hhLogger;
HHCentral *hhCentral;

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
    mainLcd.drawStr(10, 32, dtostrf((double)readVoltage / 1000, 6, 3, buffer));
    if (readVoltage / 1000 < 10)
      mainLcd.drawStr(10, 32, "0");
    //Current
    mainLcd.drawStr(10, 48, dtostrf((double)readCurrent / 1000, 6, 3, buffer));
    if (readCurrent / 1000 < 10)
      mainLcd.drawStr(10, 48, "0");

    mainLcd.setFont(MMDigit08);
    //Temp
    mainLcd.drawStr(89, 12, dtostrf((double)readTemperature / 10, 4, 1, buffer));
    //Power
    mainLcd.drawStr(89, 24, dtostrf((double)readVoltage * readCurrent / 1000000, 4, 1, buffer));
    //Total mAh
    dtostrf(totalmAh / 3600000, 7, 1, buffer);
    mainLcd.drawStr(71, 36, buffer);
    //Time & mAh
    _rtcTimer.getTime(buffer);
    mainLcd.drawStr(89, 48, buffer);

    //OnOff
    //mainLcd.drawStr(60, 12, state & STATE_ONOFF ? "On" : "Off");

    //Cell count alert
    if (state & STATE_SUSPICIOUS_CELL_COUNT)
      mainLcd.drawStr(69, 12, "!");

    //Units
    mainLcd.drawStr(50, 30, "V");
    mainLcd.drawStr(50, 46, "A");
    mainLcd.drawStr(111, 24, "W");
    mainLcd.drawStr(111, 11, "\xB0\x43"); //°C
    mainLcd.drawGlyph(124, 11, 3 + fanLevelState);
    mainLcd.drawStr(111, 36, "mAh");
  }

  menu->Print();

  //Alerts
  if (alerts != 0)
  {
    uint8_t brd = 9;
    mainLcd.setDrawColor(0);
    mainLcd.drawBox(brd, brd, 128 - 2 * brd, 64 - 2 * brd);
    mainLcd.setDrawColor(1);
    brd += 1;
    mainLcd.drawFrame(brd, brd, 128 - 2 * brd, 64 - 2 * brd);
    brd += 2;
    mainLcd.drawFrame(brd, brd, 128 - 2 * brd, 64 - 2 * brd);
    if (alerts & ALERT_MAX_CURRENT)
      mainLcd.drawStr(23, 27, "Max Current !!!");
    if (alerts & ALERT_MAX_VOLTAGE)
      mainLcd.drawStr(23, 27, "Max Voltage !!!");
    else if (alerts & ALERT_MAXTEMP)
      mainLcd.drawStr(23, 27, " Max Temp !!!");
    else if (alerts & ALERT_CUTOFF)
      mainLcd.drawStr(23, 27, "Cut Off Reached");
    else if (alerts & ALERT_SUSPICIOUS_CELL_COUNT)
      mainLcd.drawStr(23, 27, "Cell count !!!");
    mainLcd.drawStr(20, 42, "[Press Rotary B]");
  }

  mainLcd.sendBuffer();
  lcdRefreshMask = UM_NONE;
}

void loadButton_click()
{
  debug_print(F("OnOff Button click\n"));
  iddleSince = millis();
  if (alerts)
    return;
  if (state & STATE_ONOFF)
    LoadOff();
  else
    LoadOn();
}

void loadButton_longClick()
{
  debug_print(F("OnOff Button long clicked\n"));
  iddleSince = millis();
  if (alerts)
    return;
  if (!(state & STATE_ONOFF))
  {
    _rtcTimer.reset();
    totalmAh = 0;
  }
}

void vsenseButton_click()
{
  iddleSince = millis();
  state ^= STATE_VSENSE_EXT;
  debug_printb(F("VSense Button pressed, new state:"), "%2X\n", state);
  digitalWrite(P_VSENSE_EXT, (state & STATE_VSENSE_EXT) ? HIGH : LOW);
}

void selfTest()
{
  debug_print(F("Self test starting\n"));
  mainLcd.clearBuffer();
  mainLcd.setFont(MMDigit08);
  uint8_t selfTestResult = 0;
  mainLcd.drawStr(10, 8, "= Electronic  Load =");

  //Check ADC Presence
  debug_print(F("Testing ADC..."));
  mainLcd.drawStr(0, 20, "ADC...");
  mainLcd.sendBuffer();
  Wire.requestFrom(ADC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x01;
  debug_printa("%02X \n", selfTestResult);
  delay(500);
  mainLcd.drawStr(30, 20, selfTestResult & 0x01 ? "NOK" : "OK");
  mainLcd.sendBuffer();

  //Check DAC Presence
  debug_print(F("Testing DAC.."));
  delay(200);
  mainLcd.drawStr(64, 20, "DAC...");
  mainLcd.sendBuffer();
  Wire.requestFrom(DAC_ADDR, 1);
  delay(1);
  if (!Wire.available())
    selfTestResult |= 0x02;
  delay(500);
  mainLcd.drawStr(94, 20, selfTestResult & 0x02 ? "NOK" : "OK");
  mainLcd.sendBuffer();
  debug_printa("%02X \n", selfTestResult);

  //Check RTC Presence
  debug_print(F("Testing RTC.."));
  delay(200);
  mainLcd.drawStr(0, 30, "RTC...");
  mainLcd.sendBuffer();
  Wire.beginTransmission(RTC_ADDR);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
  Wire.requestFrom(RTC_ADDR, 1);
  if (!Wire.available())
    selfTestResult |= 0x04;
  delay(500);
  mainLcd.drawStr(30, 30, selfTestResult & 0x04 ? "NOK" : "OK");
  mainLcd.sendBuffer();
  debug_printa("%02X \n", selfTestResult);

  //Check IOX
  debug_print(F("Testing IO Ext.."));
  delay(200);
  mainLcd.drawStr(64, 30, "IOEx...");
  mainLcd.sendBuffer();
  Wire.beginTransmission(IOE_ADDR);
  Wire.write(uint8_t(0));
  Wire.endTransmission();
  Wire.requestFrom(IOE_ADDR, 1);
  if (!Wire.available())
    selfTestResult |= 0x08;
  delay(500);
  mainLcd.drawStr(94, 30, selfTestResult & 0x08 ? "NOK" : "OK");
  mainLcd.sendBuffer();
  debug_printa("%02X \n", selfTestResult);

  //Check Flash
  debug_print(F("Testing Flash.."));
  delay(200);
  mainLcd.drawStr(0, 40, "Flash...");
  mainLcd.sendBuffer();
  if (!flash.initialize())
    selfTestResult |= 0x10;
  delay(500);
  mainLcd.drawStr(30, 40, selfTestResult & 0x10 ? "NOK" : "OK");
  mainLcd.sendBuffer();
  debug_printa("%02X \n", selfTestResult);

  //Chech RFM
  debug_print(F("Testing RFM.."));
  delay(200);
  mainLcd.drawStr(0, 50, "RFM...");
  mainLcd.sendBuffer();
  int hhcerr = hhCentral->connect(true, 2000);
  if (hhcerr != HHCNoErr)
    selfTestResult |= 0x20;
  delay(500);
  char buffer[10];
  if (selfTestResult & 0x20)
    sprintf(buffer, "NOK[%d]", hhcerr);
  else
    sprintf(buffer, "OK[%d]", hhCentral->NodeId());
  mainLcd.drawStr(30, 50, buffer);

  mainLcd.sendBuffer();
  debug_printa("%02X \n", selfTestResult);

  mainLcd.sendBuffer();
  debug_printa("SeltTest Result : %02X\n", selfTestResult);

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
    if (state & STATE_ONOFF)
    {
      alerts = suspicious ? alerts | ALERT_SUSPICIOUS_CELL_COUNT : alerts & ~STATE_SUSPICIOUS_CELL_COUNT;
    }
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
      newVoltage = adcValue * 3.125;
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
      //newCurrent = U/R = adcValue * (2048.0 / 32768.0)/4(gain) / (10000*settings->r17Value);
      newCurrent = adcValue * 156.25 / settings->r17Value;
      if (newCurrent != readCurrent || lastCurrentUpdate + 500 < millis())
      {
        if (newCurrent != readCurrent)
        {
          debug_printb(F("Updated Current Reading :"), "adcV:%ld -> Current:%d mA\n", adcValue, newCurrent);
        }
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

void adjustFanSpeed()
{
  static int fanLevelPwm[] = {0, 128, 255};
  static uint8_t previousLevel = -1;
  switch (fanLevelState)
  {
  case 0:
    if (readTemperature > settings->fanTemps[0])
      setFanLevel(1);
    break;
  case 1:
    if (readTemperature < settings->fanTemps[0] - settings->fanHysteresis)
      setFanLevel(0);
    else if (readTemperature > settings->fanTemps[1])
      setFanLevel(2);
    break;
  case 2:
    if (readTemperature < settings->fanTemps[1] - settings->fanHysteresis)
      setFanLevel(1);
    break;
  }
  if (fanLevelState != previousLevel)
  {
    int newPwmValue = fanLevelPwm[fanLevelState];
    debug_printb(F("New Fan Level"), "%d / %d\n", fanLevelState, newPwmValue);
    previousLevel = fanLevelState;
    digitalWrite(P_FAN_ONOFF, fanLevelState > 0);
    analogWrite(P_FAN_PWM, newPwmValue);
  }
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
  debug_print(F("Serial initialized @ 9600 baud \n"));
  // initialize the lcd
  debug_print(F("Initializing LCD\n"));
  mainLcd.begin();
  mainLcd.clearBuffer();

  pinMode(P_LM35, INPUT);
  pinMode(P_FAN_ONOFF, OUTPUT);
  pinMode(P_FAN_PWM, OUTPUT);
  digitalWrite(P_FAN_ONOFF, LOW);
  pinMode(P_LOADON_LED, OUTPUT);
  pinMode(P_VSENSE_EXT, OUTPUT);
  digitalWrite(P_VSENSE_EXT, state & STATE_VSENSE_EXT ? HIGH : LOW);
  pinMode(P_TRIGGER, INPUT_PULLUP);
  debug_print(F("Pin configured\n"));

  //Setup Encoder
  encoder = new ClickEncoder(P_ENC1, P_ENC2, P_ENCBTN, ENC_STEPS);
  Timer3.initialize(1000);
  Timer3.attachInterrupt(timerIsr);
  encoder->setAccelerationEnabled(true);
  encoder->setDoubleClickEnabled(false);
  encoder->setHoldTime(500);
  debug_print(F("Encoder setup.\n"));

//Setup Load button
#if DEBUG_BOARD
  pushButton = new OneButton(P_LOADON_BTN, 1, true);
#else
  loadOnOffButton = new OneButton(P_LOADON_BTN, 0, false);
#endif
  loadOnOffButton->setClickTicks(400);
  loadOnOffButton->setPressTicks(600);
  loadOnOffButton->attachClick(loadButton_click);
  loadOnOffButton->attachLongPressStart(loadButton_longClick);
  debug_print(F("Load button setup.\n"));

  //Setup VSense Button
  vsenseButton = new OneButton(P_VSENSE_BTN, 0, false);
  vsenseButton->attachClick(vsenseButton_click);
  debug_print(F("VSense Button setup.\n"));

  //HHCentral
  hhLogger = new HHLogger(LogMode::Text);
  hhCentral = new HHCentral(hhLogger, NodeType::ElectronicLoad, "1234567", HHEnv::Dev);

  selfTest();

  MCP342x::generalCallReset();
  delay(1); // MC342x needs 300us to settle, wait 1ms
  debug_print(F("General Call Reset\n"));

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

  loadOnOffButton->tick();
  vsenseButton->tick();
  actuateReadings();

  //Alerts
  if (state & STATE_ONOFF && readTemperature > settings->fanTemps[2])
  {
    alerts |= ALERT_MAXTEMP;
    LoadOff();
  }
  if (readCurrent > settings->maxCurrent)
  {
    alerts |= ALERT_MAX_CURRENT;
    LoadOff();
  }
  if (readVoltage > settings->maxVoltage)
  {
    alerts |= ALERT_MAX_VOLTAGE;
    LoadOff();
  }
  if (state & STATE_ONOFF && settings->mode == MODE_BA)
  {
    int16_t cutOffVoltage = settings->battCellCount * settings->battCutOff[settings->battType];
    if (readVoltage < cutOffVoltage)
    {
      alerts |= ALERT_CUTOFF;
      LoadOff();
    }
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
    if (!alerts)
      menu->EncoderInc(enc);
  }

  //Encoder Button
  buttonState = encoder->getButton();
  if (buttonState == ClickEncoder::Clicked)
  {
    iddleSince = millis();
    if (alerts)
      alerts = 0;
    else
      menu->Click();
  }
  else if (buttonState == ClickEncoder::Held && oldState != ClickEncoder::Held)
  {
    iddleSince = millis();
    if (!alerts)
      menu->LongClick();
  }
  oldState = buttonState;

  //HHCentral
  Command *cmd = hhCentral->check();
  if (cmd != nullptr && cmd->msgType == CMD_LXI)
  {
    LxiCommand *lxiCmd = (LxiCommand *)cmd;
    debug_printb(F("Lxi Command Received:"), "%s\n", lxiCmd->Command);
    if (strcmp(lxiCmd->Command, "ON") == 0)
    {
      LoadOn();
    }
    else if (strcmp(lxiCmd->Command, "OFF") == 0)
    {
      LoadOff();
    }
    else if (strncmp(lxiCmd->Command, "CC:", 3) == 0)
    {
      int ma = atoi(lxiCmd->Command + 3);
      if (settings->mode != 0)
      {
        setWorkingModeMenuItem->SetCurrentChoiceIndex(0);
        menu_modeChanged(0);
      }
      setValueMenuItem->SetValue(ma);
      menu_setValueChanged(ma);
    }
    else if (strcmp(lxiCmd->Command, "VA?") == 0)
    {
      VoltAmperReport vaRpt;
      vaRpt.Tension = readVoltage;
      vaRpt.Current = readCurrent;
      hhCentral->send(&vaRpt);
    }
  }

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