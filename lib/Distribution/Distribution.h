#ifndef _DISTRIBUTION_H_
#define _DISTRIBUTION_H_

#include <Arduino.h>

class Distribution
{
public:
    Distribution(uint8_t size);
    void Clear();
    void Add(uint8_t value);
    uint32_t Instances(uint8_t value);
    void Print();
    uint16_t samples = 0;

private:
    void Sort();
    uint8_t _fillLevel = 0;
    uint8_t _size;
    uint32_t *_storage = NULL;
};

Distribution::Distribution(uint8_t size)
{
    _size = size;
    Clear();
}

void Distribution::Clear()
{
    free(_storage);
    _storage = (uint32_t *)malloc(_size * sizeof(uint32_t));
    _fillLevel = 0;
    samples = 0;
}

void Distribution::Add(uint8_t value)
{
    samples++;
    uint8_t index = 0;
    while (index < _fillLevel && (_storage[index] & 0xFF) != value)
        index++;
    if (index == _fillLevel)
    {
        if (_fillLevel >= _size)
        {
            uint32_t *na = (uint32_t *)malloc((_size + 5) * sizeof(uint32_t));
            memcpy(na, _storage, _size * sizeof(uint32_t));
            free(_storage);
            _storage = na;
            _size += 5;
        }
        _storage[_fillLevel] = value;
        _fillLevel++;
        Sort();
    }
    _storage[index] += (1 << 8);
}

void Distribution::Sort()
{
    if (_fillLevel <= 1)
        return;
    uint32_t temp;
    for (uint8_t i = 1; i < _fillLevel; i++)
    {
        temp = _storage[i];
        int8_t ni = i - 1;
        while (ni >= 0 && (_storage[ni] & 0xFF) > (temp & 0xFF))
        {
            _storage[ni + 1] = _storage[ni];
            ni--;
        }
        _storage[ni + 1] = temp;
    }
}

uint32_t Distribution::Instances(uint8_t value)
{
    uint8_t index = 0;
    while (index < _fillLevel && (_storage[index] & 0xFF) != value)
        index++;
    if (index == _fillLevel)
        return 0;
    return _storage[index] >> 8;
}

void Distribution::Print()
{
    for (uint8_t i = 0; i < _fillLevel; i++)
    {
        Serial.print("[");
        Serial.print(_storage[i] & 0xFF);
        Serial.print("]:");
        Serial.print(_storage[i] >> 8);
        Serial.print(" ");
    }
    Serial.println("");
}

#endif