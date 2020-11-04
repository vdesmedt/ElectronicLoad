#ifndef _HEADER_H_
#define _HEADER_H_

#include <Wire.h>
#include <SPI.h>
#include <Arduino.h>
#include <ClickEncoder.h>
#include <OneButton.h>
#include <TimerOneThree.h>
#include <MCP4725.h>
#include <MCP342x.h>
#include <PacketSerial.h>
#include <Menu.h>
#include <Filters/SMA.hpp>
#include <SPIFlash.h>
#include <Adafruit_MCP23008.h>
#include <MemoryFree.h>
#include <Settings.h>
#include <debug.h>
#include <MCP79410_Timer.h>
#include <Distribution.h>
#include <U8g2lib.h>
#include <HHLogger.h>
#include <HHCentral.h>
#include <HhMessages.h>

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
void LoadOff();
void LoadOn();

#include <specialLcdChar.h>
#include "menumg.h"

#endif