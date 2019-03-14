# EEPROM
Utilities for reading and writing text strings and binary data from I2C EEPROMs and memory.

## All EEPROM class utilities must implement the following arguments / operations
switch|argument|description
---|---|---
--capacity|(none)|print bytes of EEPROM capacity to stdout and exit
--read|filename|read EEPROM and write to `filename`
--write|filename|write contents of `filename` to EEPROM
--dump|(none)|dump entire contents of EEPROM to stdout
--string|(none)|null terminate contents to write. Read EEPROM until null encountered
--n-bytes|bytes|read/write up to `n-bytes`
--start-address|address|starting EEPROM address
--i2c-device|device|`/dev/` entry for I2C-dev device
--i2c-address|chip address|hex address of chip

### Example: Write output of ifconfig to EEPROM as a string, starting at address 128, with 2048 byte limit
```
mwp@raspberrypi:~/aprsI2C/eeprom $ ifconfig | ./eeprom_2464 --string --write /dev/stdin --start-address 128 --n-bytes 2048 
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

### Example: Read that back and write to a file called test2
````
mwp@raspberrypi:~/aprsI2C/eeprom $ ./eeprom_2464 --string --read test2 --start-address 128  --n-bytes 2048
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
