# RTC (Real Time Clock)

All RTC utilities can read a time and set a time. APRS World uses ISO 8601 format. Speficially the MySQL style with a `' '` between date and time. `yyyy-MM-dd HH:mm:ss`

Linux date command to print in this format:
`date +"%Y-%m-%d %k:%M:%S"`

Example Linux date command to set in this format:
`date --set "2017-02-14 17:27:09"`

## All RTC class utilities must implement the following arguments / operations
switch|argument|description
---|---|---
--read|(none)|reads time from RTC and prints to stdout
--set |date and time|sets RTC to argument date and time
--i2c-device|device|`/dev/` entry for I2C-dev device
--i2c-address|chip address|hex address of chip

### Example: Set RTC time to system time
```
./rtc_ds1307 --set "`date +"%Y-%m-%d %k:%M:%S"`"
```
### Example: Set system time to RTC time
```
date --set "`./rtc_ds1307 --read 2>/dev/null`"
```


## DS1307 (rtc/rtc_ds1307.c)
RTC with 56 bytes of general purpose battery backed RAM. 

In addition to required arguments / operations, it also supports

switch|argument|description
---|---|---
--ram-read|(none)|read contents of RAM bytes and print to stdout. Stop at first '\0'
--ram-set|string|write string to RAM
--dump|(none)|dump all 64 bytes of RTC and RAM to stdout




