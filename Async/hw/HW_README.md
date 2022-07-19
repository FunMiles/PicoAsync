# Raspberry Pi Pico Hardware
The *async_hardware* library offers asynchronous interface to the following hardware components:
- DMA Channels
- UART

All its public API is within the `async:hw` namespace.

## DMA Channel

DMA Channels can be used in read and/or write mode.
Each mode has a separate configuration. As of this version,
a channel can be configured for read only mode or for
read and write mode. Should you need write only, create a dummy
read configuration.

The `SSD1306` demo has an example of write-only mode usage while
the `DS1820_DMA` demo makes use of both read and write mode.


## UART

The UART driver offers writing to a UART using DMA in write mode while
reading is supported via a interrupt based driver.

