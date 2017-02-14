# APRS World I2C device utilities

## Introduction

Utilities for communicating with various I2C devices APRS World uses on our boards. 

Communications through the Linux kernel i2c-dev module. Documentation at:
https://www.kernel.org/doc/Documentation/i2c/dev-interface


## RTC (Real Time Clock)

All RTC utilities can read a time and set a time. APRS World uses ISO 8601 format. Speficially the MySQL style with a `' '` between date and time. `yyyy-MM-dd HH:mm:ss`

Linux date command to print in this format:
`date +"%Y-%m-%d %k:%M:%S"`

Example Linux date command to set in this format:
`date --set "2017-02-14 17:27:09"`

### All RTC class utilties must implement the following arguments / operations
switch|argument|description
---|---|---
--read|(none)|reads time from RTC and prints to stdout
--set |date and time|sets RTC to argument date and time
--i2c-device|device|`/dev/` entry for I2C-dev device
--i2c-address|chip address|hex address of chip


#### DS1307



