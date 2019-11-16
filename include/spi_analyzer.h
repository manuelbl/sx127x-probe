/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * SPI communication analyzer
 */

#ifndef _SPI_ANALYZER_H_
#define _SPI_ANALYZER_H_

#include <stdint.h>
#include <stddef.h>
#include "timing_analyzer.h"

class SpiAnalyzer
{
public:
    SpiAnalyzer(const uint8_t* buf, size_t bufSize, TimingAnalyzer ta)
        : timingAnalyzer(ta), circularBufferStart(buf), circularBufferEnd(buf + bufSize) {}
    void OnTrx(uint32_t time, const uint8_t *startTrx, const uint8_t *endTrx);

private:
    void OnRegWrite(uint32_t time, uint8_t reg, uint8_t value);
    void OnOpmodeChanged(uint32_t time, uint8_t value);

private:
    TimingAnalyzer &timingAnalyzer;
    const uint8_t* circularBufferStart;
    const uint8_t* circularBufferEnd;
};

#endif
