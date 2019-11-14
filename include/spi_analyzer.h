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
#include "timing_analyzer.h"

class SpiAnalyzer
{
public:
    SpiAnalyzer(TimingAnalyzer ta) : timingAnalyzer(ta) {}
    void AnalyzeTrx(uint32_t time, const uint8_t *startTrx, const uint8_t *endTrx,
                    const uint8_t *startBuf, const uint8_t *endBuf);

private:
    void ProcessRegWrite(uint32_t time, uint8_t reg, uint8_t value);
    void ProcessOpmodeChange(uint32_t time, uint8_t value);

private:
    TimingAnalyzer &timingAnalyzer;
};

#endif
