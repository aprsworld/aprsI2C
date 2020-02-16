# APRS World I2C device utilities

## Introduction

Utilities for communicating with various I2C devices that APRS World uses on our boards. 

Communications through the Linux kernel i2c-dev module. Documentation at:
https://www.kernel.org/doc/Documentation/i2c/dev-interface
The support utilities and header files are provided by the Debian package `libi2c-dev`

Utilities are, by default, verbose. All debugging output is sent to stderr. To not get these message, stderr can re-directed to `/dev/null` using `2>/dev/null`. Or to combine stderr and stdout use `2>&1` in bash.

## Raspberry PI useage

Enable I2C using `sudo raspi-config`

If non-root users are to be allowed, they must be in the `i2c` group. Add user to the group with `adduser user i2c` where `user` is the username you wish to add.

## Optional support tools 
Install Linux I2C support utilities with `sudo apt-get install -y i2c-tools`

Scan for I2C devices on bus with `sudo i2cdetect -y 1`

```
root@aprscam-unconfigured:/home/aprs/aprsI2C/aprs# i2cdetect -y 1
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- 1a -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
50: 50 -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
70: -- -- -- -- -- -- -- --
```



## EEPROM
Utilities for reading and writing text strings and binary data from I2C EEPROMs and memory.

## RTC (Real Time Clock)

All RTC utilities can read a time and set a time. APRS World uses ISO 8601 format. Speficially the MySQL style with a `' '` between date and time. `yyyy-MM-dd HH:mm:ss`

## Sensors

Sensor ICs such as pressure and inertial measurement.
