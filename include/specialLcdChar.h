#include <Arduino.h>
#include <LiquidCrystal_PCF8574.h>

#define SC_THERMO_L0 2
#define SC_THERMO_L1 3
#define SC_THERMO_L2 4
#define SC_THERMO_L3 5
void setupSPecialLcdChars(LiquidCrystal_PCF8574 *lcd1)
{
    int thermo_l0[]{
        B00100,
        B01010,
        B01010,
        B01010,
        B01010,
        B10001,
        B10001,
        B01110};
    int thermo_l1[]{
        B00100,
        B01010,
        B01010,
        B01010,
        B01010,
        B10001,
        B11111,
        B01110};

    int thermo_l2[]{
        B00100,
        B01010,
        B01010,
        B01010,
        B01110,
        B11111,
        B11111,
        B01110};

    int thermo_l3[]{
        B00100,
        B01110,
        B01110,
        B01110,
        B01110,
        B11111,
        B11111,
        B01110};

    lcd1->createChar(SC_THERMO_L0, thermo_l0);
    lcd1->createChar(SC_THERMO_L1, thermo_l1);
    lcd1->createChar(SC_THERMO_L2, thermo_l2);
    lcd1->createChar(SC_THERMO_L3, thermo_l3);
}