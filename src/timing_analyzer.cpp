/*
 * SX127x Probe - STM32F1x software to monitor LoRa timings
 * 
 * Copyright (c) 2019 Manuel Bleichenbacher
 * Licensed under MIT License
 * https://opensource.org/licenses/MIT
 * 
 * LoRa timing analyzer
 */

#include <math.h>
#include <stdio.h>
#include "main.h"
#include "timing_analyzer.h"
#include "uart.h"


#define TIMESTAMP_PATTERN "%10lu: "


void TimingAnalyzer::StartTx(uint32_t time)
{
    PrintTimestamp(time);
    uartPrint("TX start\r\n");
}

void TimingAnalyzer::StartRx(uint32_t time)
{
    PrintTimestamp(time);
    uartPrint("RX start\r\n");
}

void TimingAnalyzer::PrintTimestamp(uint32_t timestamp)
{
    timestamp = (uint32_t)lround(timestamp * TIMING_CORR);
    char buf[sizeof(TIMESTAMP_PATTERN) + 7];
    snprintf(buf, sizeof(buf), TIMESTAMP_PATTERN, timestamp);
    uartPrint(buf);
}

void TimingAnalyzer::DoneInterrupt(uint32_t time)
{
    PrintTimestamp(time);
    uartPrint("Done\r\n");
}

void TimingAnalyzer::TimeoutInterrupt(uint32_t time)
{
    PrintTimestamp(time);
    uartPrint("Timeout\r\n");
}
