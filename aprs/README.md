# APRS
Utilities for reading and writing APRS World I2C devices

## All utilities must implement the following arguments / operations
switch|argument|description
---|---|---
--i2c-device|device|`/dev/` entry for I2C-dev device
--i2c-address|chip address|hex address of chip
--help|(none)|print help message

## pzPowerI2C

switch|argument|description
---|---|---
--power-host-on|seconds|turn on power switch to host (ie Pi) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--power-host-off|seconds|turn on power switch to host (ie Pi) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--power-net-on|seconds|turn on power switch to external USB device (ie network adapter) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--power-net-off|seconds|turn on power switch to external USB device (ie network adapter) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--read|(none)|read current state and send to stdout in JSON format
--read-switch|(none)|read state of magnetic switch and latch and set exit value (see [--read-switch return code](#--read-switch-return-code)
--reset-switch-latch|(none)|clear the latch of the magnetic switch

### --read-switch Return Code
exit value|description
---|---
0|Success
1|Error
2|Timeout
3|not applicable
4|Magnetic switch active


01234567

Bits
76543210
       ^ success / error
      ^  timeout / not timeout
     ^   magnetic switch active / magnetic switch not active
    ^    magnetic latch set / magnetic latch not set
