# esp32-ds18b20-example

[![Platform: ESP-IDF](https://img.shields.io/badge/ESP--IDF-v3.0%2B-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/)
[![Build Status](https://travis-ci.org/DavidAntliff/esp32-ds18b20-example.svg?branch=master)](https://travis-ci.org/DavidAntliff/esp32-ds18b20-example)
[![license](https://img.shields.io/github/license/mashape/apistatus.svg)]()

## Introduction

This is an example application for the Maxim Integrated DS18B20 Programmable Resolution 1-Wire Digital Thermometer 
device.

It supports a single or multiple devices on the same 1-Wire bus.

It is written and tested for v3.3 and v4.1-beta1 of the [ESP-IDF](https://github.com/espressif/esp-idf) environment,
using the xtensa-esp32-elf toolchain (gcc version 5.2.0, crosstool-ng-1.22.0-80-g6c4433a).

Ensure that submodules are cloned:

    $ git clone --recursive https://github.com/DavidAntliff/esp32-ds18b20-example.git

Build the application with:

    $ cd esp32-ds18b20-example
    $ idf.py menuconfig    # set your serial configuration and the 1-Wire GPIO - see below
    $ idf.py build
    $ idf.py -p (PORT) flash monitor

The program should detect your connected devices and periodically obtain temperature readings from them, displaying them
on the console.

## Dependencies

This application makes use of the following components (included as submodules):

 * components/[esp32-owb](https://github.com/DavidAntliff/esp32-owb)
 * components/[esp32-ds18b20](https://github.com/DavidAntliff/esp32-ds18b20)

## Hardware

To run this example, connect one or more DS18B20 devices to a single GPIO on the ESP32. Use the recommended pull-up 
resistor of 4.7 KOhms, connected to the 3.3V supply.

`idf.py menuconfig` can be used to set the 1-Wire GPIO.

If you have several devices and see occasional CRC errors, consider using a 2.2 kOhm pull-up resistor instead. Also 
consider adding decoupling capacitors between the sensor supply voltage and ground, as close to each sensor as possible.

If you wish to enable a second GPIO to control an external strong pull-up circuit for parasitic power mode, ensure 
`CONFIG_ENABLE_STRONG_PULLUP=y` and `CONFIG_STRONG_PULLUP_GPIO` is set appropriately.
 
See documentation for [esp32-ds18b20](https://www.github.com/DavidAntliff/esp32-ds18b20-example#parasitic-power-mode)
for further information about parasitic power mode, including strong pull-up configuration.


## Features

This example provides:

 * External power supply detection.
 * Parasitic power supply detection.
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
