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

#define LOGGIN_MODE_COUNT 4
#define LOG_OFF 0
#define LOG_SERIAL 1
#define LOG_PACKET_SERIAL 2
#define LOG_RFM69 3

#define WORKINGMODE_COUNT 4
#define MODE_CC 0
#define MODE_CR 1
#define MODE_CP 2
#define MODE_BA 3

#define FLASH_ADR 0x20000 //First memory availble after reserved for OTA update

#define LCD_TEMP_MAX_UPDATE_RATE 1000

#define UM_TEMP (1 << 0)
#define UM_VOLTAGE (1 << 1)
#define UM_CURRENT (1 << 2)
#define UM_LOAD_ONOFF (1 << 3)
#define UM_TIME (1 << 4)
#define UM_ALERT (1 << 5)

#define STATE_ONOFF 1
#define STATE_SUSPICIOUS_CELL_COUNT (1 << 3)

#define BATT_TYPE_COUNT 4

#define TRIGGER_TYPE_COUNT 3
#define TRIGGER_OFF 0
#define TRIGGER_FLIP 1
#define TRIGGER_TIMER 2

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

#define SC_BATT 7
#define SC_MENU_UP 1
#define SC_THERMO_L0 2
#define SC_THERMO_L1 3
#define SC_THERMO_L2 4
#define SC_THERMO_L3 5
#define SC_WATCH 6
