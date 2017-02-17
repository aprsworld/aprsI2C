# APRS World I2C device utilities

## Introduction

Utilities for communicating with various I2C devices APRS World uses on our boards. 

Communications through the Linux kernel i2c-dev module. Documentation at:
https://www.kernel.org/doc/Documentation/i2c/dev-interface
The support utilities and header files are provided by the Debian package `libi2c-dev`

Utilities are, by default, verbose. All debugging output is sent to stderr. To not get these message, stderr can re-directed to `/dev/null` using `2>/dev/null`. Or to combine stderr and stdout use `2>&1` in bash.

## EEPROM
Utilities for reading and writing text strings and binary data from I2C EEPROMs and memory.

## RTC (Real Time Clock)

All RTC utilities can read a time and set a time. APRS World uses ISO 8601 format. Speficially the MySQL style with a `' '` between date and time. `yyyy-MM-dd HH:mm:ss`
