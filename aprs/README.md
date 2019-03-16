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
--read-switch|(none)|read state of magnetic switch and latch and set exit value (see [--read-switch Exit Status](#--read-switch-exit-status))
--reset-switch-latch|(none)|clear the latch of the magnetic switch

### --read-switch Exit Status
exit status|description
---|---
0|undefined (should not happen)
1|error
2|Magnetic switch not active AND magnetic latch not set
3|Magnetic switch active AND magnetic latch not set (should not happen)
4|Magnetic switch not active AND magnetic latch set
5|Magnetic switch active AND magnetic latch set
