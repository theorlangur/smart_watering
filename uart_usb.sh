#!/bin/sh
#/dev/ttyUSB0 - depending on where the CH340 USB-to-UART will be detected
# minicom -b 921600 -D /dev/nrf54l15
minicom -b 115200 -D /dev/nrf54l15
