/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * LoRa timing analyzer
 */

#ifndef _TIMING_ANALYZER_H_
#define _TIMING_ANALYZER_H_

#include <stdint.h>

class TimingAnalyzer
{
public:
    void StartTx(uint32_t time);
    void StartRx(uint32_t time);
    void DoneInterrupt(uint32_t time);
    void TimeoutInterrupt(uint32_t time);

private:
    void PrintTimestamp(uint32_t timestamp);
};

#endif
