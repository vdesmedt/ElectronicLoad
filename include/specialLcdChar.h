#include <Arduino.h>

#define SC_MENU_UP 1
int menuUp[]{
    B00100,
    B01110,
    B11111,
    B00100,
    B11100,
    B00000,
    B00000,
    B00000};
#define SC_THERMO_L0 2
int thermo_l0[]{
    B00100,
    B01010,
    B01010,
    B01010,
    B01010,
    B10001,
    B10001,
    B01110};
#define SC_THERMO_L1 3
int thermo_l1[]{
    B00100,
    B01010,
    B01010,
    B01010,
    B01010,
    B10001,
    B11111,
    B01110};
#define SC_THERMO_L2 4
int thermo_l2[]{
    B00100,
    B01010,
    B01010,
    B01010,
    B01110,
    B11111,
    B11111,
    B01110};
#define SC_THERMO_L3 5
int thermo_l3[]{
    B00100,
    B01110,
    B01110,
    B01110,
    B01110,
    B11111,
    B11111,
    B01110};