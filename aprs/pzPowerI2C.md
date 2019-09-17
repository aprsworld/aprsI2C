# pzPowerI2C

The default I2C address for pzPowerI2C board is `0x1a`. This is defined in [pzPowerI2C.h] (https://github.com/aprsworld/pzPowerI2C/blob/master/pzPowerI2C.h). The convention used in that is shift to the left one bit of what Linux sees. So `0x34` in pzPowerI2C.h `>> 1` becomes `0x1a`.

## Build requirements

`libjson-c-dev` is required. Install with `sudo apt-get install libjson-c-dev`.

Build with `make`

## Command line arguments
### options for reading status and clearing latches
<!--- 300 series -->
switch|argument|description
---|---|---
--read|(none)|read current state and send to stdout in JSON format
--read-switch|(none)|read state of magnetic switch and latch and set exit value (see [--read-switch Exit Status](#--read-switch-exit-status))
--reset-switch-latch|(none)|clear the latch of the magnetic switch. Will happen after `--read` or `--read-switch`
--reset-write-watchdog|(none)|resets the write watchdog

### options for setting power off parameters

These are used to control the turn off behavior of the host (ie Pi)

#### command and watchdog triggered
<!--- 400 series -->
switch|argument|description
---|---|---
--set-command-off|seconds|0 for no delay. 65534 seconds max delay. 65535 to cancel.
--set-command-off-hold-time|seconds for power to be off after command. 65534 seconds max
--disable-read-watchdog|(none)|Disable read watchdog
--set-read-watchdog-off-threshold|seconds|Power off after `seconds` without an I2C read to pzPowerI2C. 65535 for disable
--set-read-watchdog-off-hold-time|seconds|seconds for power to be off after watchdog is triggered. 65534 seconds max
--disable-write-watchdog|(none)|Disable write watchdog
--set-write-watchdog-off-threshold|seconds|Power off after `seconds` without watchdog write. 65535 for disable
--set-write-watchdog-off-hold-time|seconds|seconds for power to be off after watchdog is triggered. 65534 seconds max



#### voltage and temperature triggered
<!--- 500 series -->
switch|argument|description
---|---|---
--disable-lvd|(none)|Disable low voltage disconnect (LVD)
--set-lvd-off-threshold|voltage|voltage for LVD
--set-lvd-off-delay|seconds|Seconds voltage must be below threshold to trigger. 0 for no delay. 65535 to disable LVD
--disable-hvd|(none)|Disable high voltage disconnect (HVD)
--set-hvd-off-threshold|voltage|voltage for HVD
--set-hvd-off-delay|seconds|Seconds voltage must be above theshold to trigger. 0 for no delay. 65535 to disable HVD
--disable-ltd|(none)|Disable low temperature disconnect (LTD)
--set-ltd-off-threshold|temperature|temperature LTD
--set-ltd-off-delay|seconds|Seconds temperature must be below threshold to trigger. 0 for no delay. 65535 to disable LTD
--disable-htd|(none)|Disable high temperature disconnect (HTD)
--set-htd-off-threshold|temperature|temperature for HTD
--set-htd-off-delay|seconds|Seconds temperature must be above threshold to trigger. 0 for no delay. 65535 to disable HTD



### options for setting board paramaters
<!--- 10000 series -->
switch|argument|description
---|---|---
--param|value|`save` or `defaults` or `reset_cpu` or value in the range of 0 to 65535
--set-serial|new serial|serial prefix (A to Z) combined with serial number 0 to 65535
--set-adc-ticks|ticks|10 millisecond (?) ticks between ADC samples. Range TBD
--set-startup-power-on-delay|seconds|Seconds after pzPowerI2C startup before power is turned on to PI. 0 for no delay. 65535 to disable automatic power on



### --read-switch Exit Status
exit status|description
---|---
below 128|Program error.
128|Magnetic switch not active AND magnetic latch not set
129|Magnetic switch not active AND magnetic latch set
130|Magnetic switch active AND magnetic latch set
