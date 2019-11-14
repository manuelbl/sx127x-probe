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

void TimingAnalyzer::OnTxStart(uint32_t time)
{
    PrintTimestamp(time);
    Uart.Print("TX start\r\n");
}

void TimingAnalyzer::OnRxStart(uint32_t time)
{
    PrintTimestamp(time);
    Uart.Print("RX start\r\n");
}

void TimingAnalyzer::PrintTimestamp(uint32_t timestamp)
{
    timestamp = (uint32_t)lround(timestamp * TIMING_CORR);
    char buf[sizeof(TIMESTAMP_PATTERN) + 7];
    snprintf(buf, sizeof(buf), TIMESTAMP_PATTERN, timestamp);
    Uart.Print(buf);
}

void TimingAnalyzer::OnDoneInterrupt(uint32_t time)
{
    PrintTimestamp(time);
    Uart.Print("Done\r\n");
}

void TimingAnalyzer::OnTimeoutInterrupt(uint32_t time)
{
    PrintTimestamp(time);
    Uart.Print("Timeout\r\n");
}
