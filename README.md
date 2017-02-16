# APRS World I2C device utilities

## Introduction

Utilities for communicating with various I2C devices APRS World uses on our boards. 

Communications through the Linux kernel i2c-dev module. Documentation at:
https://www.kernel.org/doc/Documentation/i2c/dev-interface

Utilities are, by default, verbose. All debugging output is sent to stderr. To not get these message, stderr can re-directed to `/dev/null` using `2>/dev/null`. Or to combine stderr and stdout use `2>&1` in bash.

## EEPROM
Utilities for reading and writing text strings and binary data from I2C EEPROMs and memory.

### All EEPROM class utilities must implement the following arguments / operations
switch|argument|description
---|---|---
--capacity|(none)|print bytes of EEPROM capacity to stdout and exit
--read|filename|read EEPROM and write to `filename`
--write|filename|write contents of `filename` to EEPROM
--dump|(none)|dump entire contents of EEPROM to stdout
--string|(none)|null terminate contents to write. Read EEPROM until null encountered
--n-bytes|bytes|read/write up to `n-bytes`
--i2c-device|device|`/dev/` entry for I2C-dev device
--i2c-address|chip address|hex address of chip

#### Example: Write output of ifconfig to EEPROM as a string, starting at address 128, with 2048 byte limit
```
mwp@raspberrypi:~/aprsI2C/eeprom $ ifconfig | ./ee_2464 --string --write /dev/stdin --start-address 128 --n-bytes 2048 
# ee_2464 24AA64 / 24LC64 EEPROM I2C utility
# using I2C device /dev/i2c-1
# using I2C device address of 0x50
# input file: /dev/stdin
# start address: 128
# bytes to read / write: 2048
# input file: /dev/stdin
# write string mode (stopping at first null)
# adding null after last byte. null at address 1357.
# wrote 1230 bytes to EEPROM
# Done...
```

#### Example: Read that back and write to a file called test2
````
mwp@raspberrypi:~/aprsI2C/eeprom $ ./ee_2464 --string --read test2 --start-address 128  --n-bytes 2048
# ee_2464 24AA64 / 24LC64 EEPROM I2C utility
# using I2C device /dev/i2c-1
# using I2C device address of 0x50
# start address: 128
# bytes to read / write: 2048
# output file: test2
# read string mode (stopping before first null)
# 2048 bytes read
# Done...
````



## RTC (Real Time Clock)

All RTC utilities can read a time and set a time. APRS World uses ISO 8601 format. Speficially the MySQL style with a `' '` between date and time. `yyyy-MM-dd HH:mm:ss`

Linux date command to print in this format:
`date +"%Y-%m-%d %k:%M:%S"`

Example Linux date command to set in this format:
`date --set "2017-02-14 17:27:09"`

### All RTC class utilities must implement the following arguments / operations
switch|argument|description
---|---|---
--read|(none)|reads time from RTC and prints to stdout
--set |date and time|sets RTC to argument date and time
--i2c-device|device|`/dev/` entry for I2C-dev device
--i2c-address|chip address|hex address of chip

#### Example: Set RTC time to system time
```
./rtc_ds1307 --set "`date +"%Y-%m-%d %k:%M:%S"`"
```
#### Example: Set system time to RTC time
```
date --set "`./rtc_ds1307 --read 2>/dev/null`"
```


### DS1307 (rtc/rtc_ds1307.c)
RTC with 56 bytes of general purpose battery backed RAM. 

In addition to required arguments / operations, it also supports

switch|argument|description
---|---|---
--ram-read|(none)|read contents of RAM bytes and print to stdout. Stop at first '\0'
--ram-set|string|write string to RAM
--dump|(none)|dump all 64 bytes of RTC and RAM to stdout




