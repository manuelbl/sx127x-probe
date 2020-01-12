# SX127x Probe – STM32F1x software to monitor the LoRa timings of a Semtech SX127x chip

Software to monitor the communictaion with a Semtech SX127x chip to verify the firmware such a LMIC achieves correct LoRa timings. The software is run on a STM32F1x board (e.g. Blue Pill or Black Pill) and connected to  several pins of the LoRa board. It monitors the SPI communication and the interrupt pins, measures the timing and analyzes whether they conform to the LoRa standard.


## Connections

### Connections between probe (STM32 board) and SX127x board

| SX127x     | Probe     |
| ---------- | --------- |
| GND        | GND       |
| NSS / CS   | PB12      |
| SCLK       | PB13      |
| MOSI       | PB15      |
| DIO0       | PB3       |
| DIO1       | PB4       |

With the exception of GND, all connections are configured as inputs with no pull up/down and they are assigned to 5V tolerant pins.
So they can be connected in addition to the already existing circuitry between the SX127x chip and the MCU.
They do not affect the SX127x/LoRa board, and they work both 3.3V and 5V boards.

### Outputs

The analysis output is written to the serial connection provided via USB. No driver is needed as the serial connection is implemented as a USB CDC device class.

If the USB connection is not convenient, the output can instead be written to a UART pin, using 115,200 bps:

- PA2: TX

For the UART output, the code must be compiled with:

```
-D Serial=Uart
```

Additionally, a 1 kHz square wave is output so you can measure the accurracy of the probe clock.

- PA1: 1 kHz reference clock


## Analysis

The probe listens to the SPI communication between the MCU and the SX127x chip and tracks the settings such a spreading factor and bandwidth. Additionally, it listens for the *opmode* command that puts the transceiver into transmit or receive mode.

The time when the CS signal returns to *high* after the *opmode* command is recorded (see figure below). In addition, the time of the *done* and *timeout* interrupts (pins DIO0 and DIO1) are recorded.

For the further analysis, it is then assumed that the interrupts occur immediately after transmission (air time), reception (air time) or timeout expiration. The remaining duration is assumed to be the preceding ramp-up time, e.g. to lock the PLL to the desired frequency.

![Analysis](doc/Analysis.png)

Based on this data, the timing of the RX window is examined. The window should be scheduled such that the preamble that precedes the payload overlaps with the window. If a preamble is detected, the receiver receives the payload. Otherwise, it will stop when the timeout expires.

The typical payload length is 8 symbols. 4 or 5 symbols are usually sufficient to detect the preamble (called *minimum preamble length* in below figure). Therefore it is sufficient if somewhat more than half of the preamble fall into the window. The code currently calculates with a minimum of 6 symbols.

The probe calculates the timing margin, separately for the start and the end of the RX window. The margin must be greater than 0 and optimally the start and end margin should be about the same length. A start and end margin of 5ms means that the timing for the RX windows could be off by 5ms in either direction and a downlink message would still be successfully received.

![Timing](doc/Timing.png)

The probe prints the start and end margin for each RX window and propose a correction offset to properly center the RX window and the expected preamble.

There are two reason why they are not properly centered:

1. The timing calculation might be flawed.

2. The delay caused by the code run on the MCU, the SPI communication to change the *opmode* and the ramp-up of the transceiver might not have been fully accounted for. The delay is dependent of the type of MCU, the MCU's clock speed and the SPI speed.


## Clock calibration

The analysis accuracy depends on the STM32's clock accuracy. If the clock is not exact but stable, it can be compensated with a calibration. Using a multimeter or frequency counter, the square wave on pin PA1 can be measured. Then the macro `MEASURED_CLOCK` is set to the measured value, the code is recompiled and uploaded.

Example if a frequency of 999.31 Hz is measured:

```
build_flags = -D MEASURED_CLOCK=999.31
```

If the MCU uses an external oscillator, it is usually sufficienlty accurate to make the measurements without calibration.


## Project

The software project uses the STM32Cube HAL library and the [PlatformIO](https://platformio.org/) build system.


## Architecture

The software is divided into several parts:

- Data recording uses interrupts and DMA and mainly runs in interrupt handlers.
- Data analysis and output runs as the main code (main loop).
- USB and UART output is fully asynchronous and mainly runs in interrupt handlers.

The recorded data is written to two circular buffer:

- In the *event buffer*, the type and time stamp of the events (such a SPI transaction completed and interrupts) are recorded.
- In the *SPI buffer*, the SPI data is recorded.

The circlar buffers are read by the data analysis code.


### SPI Recording

The device is configured as an SPI slave that receives only (no transmissions). It listens on the master-to-slave communication. It does not listen to the slave-to-master communication.

The SPI peripheral is configured with DMA, a circular buffer and hardware NSS. The NSS input is additionally configured with an external interrupt. Each time the raising edge triggers it, the time and the position within the SPI buffer is recorded and written to the event buffer.


### DIO0 and DIO1 pins

The DIO0 and DIO1 pins are configured with an external interrupt. On each raising edge, the time is recorded and written to the event buffer.


### Analysis

The analysis code waits for new entries in the event buffer and processes them. SPI transactions are analyzed. If they are a commmand to put the transceiver in TX or RX_SINGLE mode, the event is output. DIO0 and DIO1 triggers are also output.

### Serial Output

Serial output is written asynchronously so it does not interfer with anything else. Both the USB and the UART code use a similar approach.

Text is first put in a fixed, circular buffer. Additionally, there is a second circular queue to manage the text chunks. Each time a chunk is added or a chunk transmission is completed, the queue is checked. If it contains further chunks, the transmission of the next chunk is started.
