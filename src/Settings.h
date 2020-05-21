#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <Arduino.h>
#include "header.h"
#include <Statistic.h>
#include <debug.h>
#include <SPIFlash.h>

#define SETTINGS_VERSION 0x05

struct Settings
{
    uint8_t version = (SETTINGS_VERSION << 1) + 0;              //LSB to 1 means dirty
    uint8_t mode = 0;                                           //CC
    uint16_t setValues[WORKINGMODE_COUNT] = {0, 1000, 50, 100}; //0mA 100Ω, 5W, 100mA
    uint8_t backlight = 0;                                      //On
    uint16_t r17Value = 1000;                                   //100mΩ
    uint8_t battType = 0;                                       //Lipo
    uint16_t battCutOff[2] = {3200, 1000};                      //3.2V 1.0V
    uint16_t fanTemps[3] = {300, 400, 600};                     //30°C
    uint8_t fanHysteresis = 10;                                 //1°C
    uint8_t triggerType = 0;                                    //Manual, FlipFlop, Timer
    uint16_t triggerTimer = 1000;                               //Milliseconds
};

extern Settings *settings;

extern SPIFlash flash;

//Returns true if settings was sucessfully found in flash
bool ReadSettings()
{
    flash.readBytes(FLASH_ADR, settings, sizeof(Settings));
    if ((settings->version >> 1) != SETTINGS_VERSION)
    {
        debug_printa("Setting Ver NoMatch %X vs %X\n", settings->version >> 1, SETTINGS_VERSION);
        free(settings);
        settings = new Settings();
        return false;
    }
    return true;
}

void SaveSettings()
{
    if (settings->version & 0x01)
    {
#if EDCL_DEBUG
        unsigned long s = micros();
#endif
        flash.blockErase4K(FLASH_ADR);
        while (flash.busy())
            ;
        settings->version &= ~1; //Set Dirty flag to zero
        flash.writeBytes(FLASH_ADR, settings, sizeof(Settings));
        while (flash.busy())
            ;
#if EDCL_DEBUG
        debug_printb(F("Flash Stats took:"), "%luµs\n", micros() - s);
#endif
    }
}

#endif