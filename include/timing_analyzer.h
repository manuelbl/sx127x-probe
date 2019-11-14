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
    void OnTxStart(uint32_t time);
    void OnRxStart(uint32_t time);
    void OnDoneInterrupt(uint32_t time);
    void OnTimeoutInterrupt(uint32_t time);

private:
    void PrintTimestamp(uint32_t timestamp);
};

#endif
