#ifndef scpiparser_h
#define scpiparser_h

#include <Arduino.h>

struct SCPIToken
{
    const char *shortName;
    size_t shortNameLength;
    const char *longName;
    size_t longNameLength;
};

class SCPIParser
{
public:
    void Parse();

private:
};

#endif