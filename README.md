# ESP32-DS18B20

## Introduction

This is an example application for the Maxim Integrated DS18B20 Programmable Resolution 1-Wire Digital Thermometer device.

It supports a single or multiple devices on the same 1-Wire bus.

It is written and tested for the [ESP-IDF](https://github.com/espressif/esp-idf) environment, using the xtensa-esp32-elf toolchain (gcc version 5.2.0).

Ensure that submodules are cloned:

    $ git clone --recursive https://github.com/DavidAntliff/esp32-ds18b20-example.git

Build the application with:

    $ cd esp32-ds18b20-example.git
    $ make menuconfig    # set your serial configuration and the 1-Wire GPIO - see below
    $ make flash monitor

The program should detect your connected devices and periodically obtain temperature readings from them, displaying them on the console.

## Hardware

To run this example, connect one or more DS18B20 devices to a single GPIO on the ESP32. Use the recommended pull-up resistor of 4.k kOhms, connected to the 3.3V supply.

`make menuconfig` can be used to set the 1-Wire GPIO.

## Features

This example provides:

 * External power supply mode.
 * Static (stack-based) or dynamic (malloc-based) memory model examples.
 * No global variables.
 * Device search.
 * Addressing optimisation for a single (solo) device on a bus.
 * CRC checks on ROM code and temperature data.
 * Programmable temperature measurement resolution (9, 10, 11 or 12-bit resolution).
 * Temperature conversion and retrieval.
 * Simultaneous conversion across multiple devices.

## Source Code

The source is available from [GitHub](https://www.github.com/DavidAntliff/esp32-ds18b20-example).

## License

The code in this project is licensed under the MIT license - see LICENSE for details.

## Links

 * [DS18B20 Datasheet](http://datasheets.maximintegrated.com/en/ds/DS18B20.pdf)
 * [Espressif IoT Development Framework for ESP32](https://github.com/espressif/esp-idf)

## Acknowledgements

"1-Wire" is a registered trademark of Maxim Integrated.
