/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * Buffer to make a rolling average
 */
#ifndef ROLLING_AVERAGE_H
#define ROLLING_AVERAGE_H

#include <stdint.h>
#include <array>

class RollingAvergage
{

    static constexpr size_t BUFFER_SIZE = 20;

public:
    int32_t Mean() const;
    int32_t Variance() const;
    void AddValue(int32_t newValue);

private:
    std::array<int32_t, BUFFER_SIZE> buffer;
    size_t currentIndex = 0;
    size_t currentSize = 0;
};

#endif