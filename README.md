# SX127x Probe – STM32F1x software to monitor the LoRa timings of a Semtech SX127x chip

Software to monitor the communictaion with a Semtech SX127x chip to verify the firmware such a LMIC achieves correct LoRa timings. The software is run on a STM32F1x board (e.g. Blue Pill or Black Pill) and connected to  several pins of the LoRa board. It monitors the SPI communication and the interrupt pins, measures the timing and analyzes whether they conform to the LoRa standard.


## Connections

### Connections between probe (STM32 board) and SX127x board

| SX127x     | Probe     |
| ---------- | --------- |
| GND        | GND       |
| NSS        | PA4       |
| SCLK       | PA5       |
| MOSI       | PA7       |
| DIO0       | PB0       |
| DIO1       | PB1       |

With the exception of GND, all connections are configured as inputs with no pull up/down. So they can be connected in addition to the already existing connections between the SX127x chip and the MCU. They do not affect the SX127x/LoRa board.

### Outputs

The result is writting to the serial output, using 115,200 bps:

- PA9: TX

Additionally, a 1 kHz square wave is output to measure the accurracy of the probe clock. If the output signal deviates from 1 kHz, it can be corrected in software (line 21 in `main.cpp`).

- PA1: 1 kHz reference clock


## Project

The software project is created using STM32Cube HAL library and [PlatformIO](https://platformio.org/).


## Architecture

The software is divided into several parts:

- Data recording uses interrupts and DMA and mainly runs in interrupt handlers.
- Data analysis and output runs as the main code (main loop).
- UART output is fully asynchronous and mainly runs in interrupt handlers.

The recorded data is written to a queue, read by the data analysis code.


### SPI Recording

The device is configured as an SPI slave that receives only (no transmissions). It listens on the master-to-slave communication. It does not look at the slave-to-master communication.

The SPI peripheral is configured with DMA, a circular buffer and hardware NSS. The NSS input is additionally configured with an external interrupt. Each time the raising edge triggers it, the time and the position within the circular buffer is recorded and written to the main queue.


### DIO0 and DIO1 pins

The DIO0 and DIO1 pins are configured with an external interrupt. On each raising edge, the time is recorded and written to the main queue.


### Analysis

The analysis code waits for new entries in the main queue and processes them. SPI transactions are analyzed. If they are a commmand to put the transceiver in TX or RX_SINGLE mode, the event is output. DIO0 and DIO1 triggers are also output.

### Output

Output is written asynchronously. The text is first put in a fixed, circular buffer. Additionally, a queue entry is created in the UART queue for the appended text chunk. Each time a chunk is added or a chunk transmission is completed, the queue is checked. If it contains further chunks, the transmission of the next chunk is started. The transmissions uses the UART in DMA mode.
