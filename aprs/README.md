# APRS
Utilities for reading and writing APRS World I2C devices

## All utilities must implement the following arguments / operations
switch|argument|description
---|---|---
--i2c-device|device|`/dev/` entry for I2C-dev device
--i2c-address|chip address|hex address of chip

## pzPowerI2C

switch|argument|description
---|---|---
--power-host-on|seconds|turn on power switch to host (ie Pi) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--power-host-off|seconds|turn on power switch to host (ie Pi) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--power-ext-network-on|seconds|turn on power switch to external USB device (ie network adapter) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--power-ext-network-off|seconds|turn on power switch to external USB device (ie network adapter) after `seconds` delay. 0 for no delay. 65534 seconds max delay.
--read|(none)|read current state and send to stdout in JSON format
