
//SCULLCOM HOBBY ELECTRONICS
//ELECTRONIC DC LOAD PROJECT
//Software Version 1

#include <SPI.h>                              //include SPI library (Serial Peripheral Interface)
#include <Wire.h>                             //include I2C library
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MCP4725.h>                 //DAC library
#include <MCP342x.h>                          //Steve Marple library avaiable from    https://github.com/stevemarple/MCP342x

Adafruit_MCP4725 dac;                         //constructor

uint8_t address = 0x68;                       //0x68 is the default address for all MCP3426 device
MCP342x adc = MCP342x(address);

//Set the pins on the I2C chip used for LCD connections
//ADDR,EN,R/W,RS,D4,D5,D6,D7
LiquidCrystal_I2C lcd(0x27,2,1,0,4,5,6,7);    //0x27 is the default I2C bus address of the backpack-see article

const byte pinA = 3;
const byte pinB = 4;

const byte CursorPos = 5;                     //digital pin 5 used to set cursor position
const byte LoadOnOff = 6;                     //digital pin 6 used to toggle load on or off
const byte constantCurrent = 7;               //digital pin 7 used to select constant current mode
const byte constantResistance = 8;            //digital pin 8 to select constant resistance mode
const byte constantPower = 9;                 //digital pin 9 to select constant power mode
const byte useSetting = 10;                   //digital pin 10 used to select current constant mode setting

const byte fan = 2;                           //digital pin 2 for fan control output
const byte temperature = A0;                  //temperature output from LM35
int temp;                                     //
int tempMin = 10;                             //temperature at which to start the fan
int tempMax = 50;                             //maximum temperature when fan speed at 100%
int fanSpeed;

int CP = 8;                                   //cursor start position

int OnOff = 0;
boolean toggle = true;
int Load = 0;                                 //sets output of DAC on or off (0 = off and 1 = on)

unsigned long controlVoltage = 0;             //used for DAC to control MOSFET

long current = 0;
long voltage = 0;

int reading = 0;                              //changed from float to int

float setCurrent = 0;
float setPower = 0;
float setResistance =0;
float setCurrentCalibrationFactor = 1;      //May use this for calibration adjustment later

float setControlCurrent = 0;

int VoltsDecimalPlaces = 3;

float ActualVoltage = 0;                      //
float ActualCurrent = 0;
float ActualPower = 0;

int setReading = 0;                           //was set a float

int ControlVolts = 0;                         //used to set output current
float OutputVoltage = 0;                      //was int

String Mode ="  ";                            //used to identify which mode
String setType ="  ";                         //selects either mA, mW or ohms
int modeSelected = 0;

int lastCount = 50;
volatile float encoderPosition = 0;
volatile unsigned long factor= 0;             //number of steps to jump
volatile unsigned long encoderMax = 9000;

//--------------------------------Interrupt Routine for Rotary Encoder------------------------
void isr()
{
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();

  if (interruptTime - lastInterruptTime > 5) {  //
    if (digitalRead(pinB) == LOW)
    {
      encoderPosition = encoderPosition - factor;
    }else{
      encoderPosition = encoderPosition + factor;
    }
    encoderPosition = min(encoderMax, max(0, encoderPosition));  // sets maximum range of rotary encoder
    lastInterruptTime = interruptTime;
  }
    }
//---------------------------------Initial Set up---------------------------------------
void setup() {
  Serial.begin(9600);  //used for testing only

 Wire.begin();                                            //join i2c bus (address optional for master)

 MCP342x::generalCallReset();                             // Reset devices
 delay(1);                                                //MC342x needs 300us to settle, wait 1ms - (may not be required)

 pinMode (pinA, INPUT);
 pinMode (pinB, INPUT);

 pinMode (LoadOnOff, INPUT_PULLUP);
 pinMode (CursorPos, INPUT_PULLUP);
 pinMode (constantCurrent, INPUT_PULLUP);
 pinMode (constantPower, INPUT_PULLUP);
 pinMode (constantResistance, INPUT_PULLUP);
 pinMode (useSetting, INPUT_PULLUP);
 pinMode (fan, OUTPUT);
 pinMode (temperature, INPUT);
 
 analogReference(INTERNAL);                             //use Arduino internal reference for tempurature monitoring

 attachInterrupt(digitalPinToInterrupt(pinA), isr, LOW);

 dac.begin(0x61);                                       //the DAC I2C address with MCP4725 pin A0 set high
 dac.setVoltage(0,false);                               //reset DAC to zero for no output current set at switch on

 lcd.begin(20, 4);                                      //set up the LCD's number of columns and rows 
 lcd.setBacklightPin(3,POSITIVE);                       // BL, BL_POL
 lcd.setBacklight(HIGH);                                //set LCD backlight on
 
 lcd.clear();                                           //clear LCD display
 lcd.setCursor(6,0);                                    //set LCD cursor to column 0, row 4
 lcd.print("SCULLCOM");                                 //print SCULLCOM to display with 5 leading spaces (you can change to your own)
 lcd.setCursor(1,1);                                    //set LCD cursor to column 0, row 1 (start of second line)
 lcd.print("Hobby Electronics");                        //print Hobby Electronics to display (you can change to your own)
 lcd.setCursor(1,2);
 lcd.print("DC Electronic Load"); //
 lcd.setCursor(0,3);
 lcd.print("Software Version 1.0"); //
 delay(3000);                                           //1500 mSec delay for intro display
 lcd.clear();                                           //clear dislay

 lcd.setCursor(0,3);
 lcd.print("LOAD OFF");                                 //indicate that LOAD is off at start up
}
//---------------------------------------------------------------------------
void loop() {
if(digitalRead(constantCurrent)== LOW) {                //check if Constant Current button pressed
  Current();                                            //if selected go to Constant Current Selected routine
}
if(digitalRead(constantPower)== LOW) {                  //check if Constant Power button pressed
  Power();                                              //if selected go to Constant Power Selected routine
}
if(digitalRead(constantResistance)== LOW) {             //check if Constant Resistance button pressed
  Resistance();                                         //if selected go to Constant Resistance Selected routine
}
  lcd.setCursor(17,1);
  lcd.print(Mode);                                      //display mode selected on LCD (CC, CP or CR)

  reading = encoderPosition;                            //read input from rotary encoder
          
  lcd.setCursor(8,1);                                   //start position of setting cusor position (indicates which digit will change)
  if (reading < 10) {                                   //ensure leading zero's are displayed
    lcd.print("000");
  } else if( reading < 100 ) {
    lcd.print("00");
  } else if (reading < 1000) {
    lcd.print("0");
  }
  lcd.print (reading);                                //show input reading from Rotary Encoder on LCD
  
  lcd.setCursor (CP, 1);                              //sets cursor position
  lcd.cursor();                                       //show cursor on LCD
  delay(10);                                          //used to test - may not be required
  lastCount = encoderPosition;                        //store encoder current position
 
  readVoltageCurrent();                               //routine for ADC to read actual Voltage and Current
  ActualReading();                                    //Display actual Voltage and Current readings
  CursorPosition();                                   //Change the cusor position and reading adjusment factor (for 10's, 100's and 1000's)

if (Mode == "CC"){  
  setCurrent = reading;                               //set current is equal to input value in mA
  setReading = setCurrent;                            //show the set current reading being used
  setControlCurrent = (setCurrent/(5000/4096))*setCurrentCalibrationFactor;
  controlVoltage = setControlCurrent;
  
}
if (Mode == "CP"){
  setPower = reading;                                 //in mW
  setReading = setPower;
  setCurrent = setPower/ActualVoltage;
  setControlCurrent = (setCurrent/(5000/4096))*setCurrentCalibrationFactor;
  controlVoltage = setControlCurrent;                 //
}

if (Mode == "CR"){
  setResistance = reading;                            //in ohms
  setReading = setResistance;
  setCurrent = (ActualVoltage*1000)/setResistance;
  setControlCurrent = (setCurrent/(5000/4096))*setCurrentCalibrationFactor;
  controlVoltage = setControlCurrent; 
}

LoadSwitch();                                             //Toggle load on/off routine
if (Load == 0){
  dac.setVoltage(0,false);                                //set DAC output voltage to 0 if Load Off selected
}else{
dac.setVoltage(controlVoltage,false);                     //set DAC output voltage for Range selected
}

fanControl();                                             //call heatsink fan control
}
//--------------------------Cursor Position-------------------------------------------------------
//Change the position
void CursorPosition(void) {
  if (digitalRead(CursorPos) == LOW) {
delay(200);                                               //simple key bounce delay  
    CP = CP + 1;
   // if (CP==9){
  // CP=CP+1;
  //  } 
  }
  if (CP>11){
    CP=8;                                                 //
  }
if (CP == 11){
  factor = 1;
}
if (CP == 10) {
  factor = 10;
}
if (CP == 9){
  factor = 100;
}
if (CP == 8) {                                             //
  factor = 1000;                                           //
}
}
//----------------------Calculate Actual Voltage and Current and display on LCD---------------------
void ActualReading(void) {
  ActualCurrent = (((current*2.048)/32767)*4);            //calculate load current
  ActualVoltage = (((voltage*2.048)/32767) * 50);         //calculate load voltage upto 100v
  ActualPower = ActualVoltage*ActualCurrent;

  if (ActualVoltage <10) {
    VoltsDecimalPlaces = 3;
  }else{
    VoltsDecimalPlaces = 2;
  }
  Serial.print("CURRENT = ");         //for testing
  Serial.println(ActualCurrent);      //for testing
  Serial.print("VOLTAGE = ");         //for testing
  Serial.println(ActualVoltage);      //for testing
 
  lcd.setCursor(0,0);
  lcd.print(ActualCurrent,3);
  lcd.print("A");
  lcd.print(" ");                                        //two spaces between actual current and voltage readings
  lcd.print(ActualVoltage,VoltsDecimalPlaces);
  lcd.print("V");
  //lcd.setCursor(0,1);
  lcd.print(" "); 
  lcd.print(ActualPower,2);
  lcd.print("W");
  lcd.print(" ");
}
//-----------------------Switch Current Load On or Off------------------------------
void LoadSwitch(void) {
 
  OnOff = digitalRead(LoadOnOff);
  if (OnOff == LOW)
  {
    if(toggle)
    {
      //digitalWrite(13, HIGH);   // set the LED on
      lcd.setCursor(0,3);
      lcd.print("LOAD ON ");
      Load = 1;
      toggle = !toggle;
    }
    else
    {
      //digitalWrite(13, LOW);    // set the LED off
      lcd.setCursor(0,3);
      lcd.print("LOAD OFF");
      Load = 0;
      toggle = !toggle;
    }
  }
  //delay(200);  //simple delay for key debounce
}
//-----------------------Select Constant Current--------------------------------
void Current(void) {
  Mode = ("CC");
lcd.setCursor(0,1);
lcd.print("                ");
lcd.setCursor(0,1);
lcd.print("Set I = ");
//lcd.setCursor(12,1);
//lcd.print("  ");
lcd.setCursor(12,1);
lcd.print("mA");
setType = ("mA");
modeSelected = 1;
}
//----------------------Select Constant Power------------------------------------
void Power(void) {
Mode = ("CP");
lcd.setCursor(0,1);
lcd.print("                ");
lcd.setCursor(0,1);
lcd.print("Set W = ");
//lcd.setCursor(12,1);
//lcd.print("  ");
lcd.setCursor(12,1);
lcd.print("mW");
setType = ("mW");
modeSelected = 2;
}
//----------------------- Select Constant Resistance---------------------------------------
void Resistance(void) {
  Mode = ("CR");
lcd.setCursor(0,1);
lcd.print("                ");
lcd.setCursor(0,1);
lcd.print("Set R = ");
//lcd.setCursor(12,1);
//lcd.print("  ");
lcd.setCursor(12,1);
lcd.print((char)0xF4);
setType = ((char)0xF4);
lcd.print(" ");
modeSelected = 3;
}
//-----------------------Read Voltage and Current---------------------------------------------
void readVoltageCurrent (void) {
  
  MCP342x::Config status;
// Initiate a conversion; convertAndRead() will wait until it can be read
  adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
           MCP342x::resolution16, MCP342x::gain1,
           1000000, voltage, status);
  
// Initiate a conversion; convertAndRead() will wait until it can be read
  adc.convertAndRead(MCP342x::channel2, MCP342x::oneShot,
           MCP342x::resolution16, MCP342x::gain1,
           1000000, current, status);
  
}
//-----------------------Fan Control-----------------------------------------------------
void fanControl (void) {
  temp = analogRead(temperature);
  temp = temp * 0.107421875; // convert to Celsius
  
  if (temp < tempMin) {      //is temperature lower than minimum setting
    fanSpeed = 0;            //fan turned off
    digitalWrite(fan, LOW);
  }
  if ((temp >= tempMin) && (temp <= tempMax)){
    fanSpeed = map(temp, tempMin, tempMax, 100, 255);
    Serial.print("Fan Speed");
    Serial.println(fanSpeed);
    analogWrite(fan, fanSpeed);
    lcd.setCursor(0,2);
    lcd.print("Heatsink Temp = ");
    lcd.print(temp);
    lcd.print((char)0xDF);
    lcd.print("C");
  }
}
//----------------------------------------------------------------------------
