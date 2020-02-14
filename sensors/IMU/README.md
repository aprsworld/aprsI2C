# imuToMQTT

Read BMP280 and LSM9DS1 (packaged on BerryIMU V3) for gyro, accelerometer and magnometer, pressure, and temperature. Send results to stdout and optiocally MQTT

## Installation


`sudo apt-get install mosquitto-dev`

`sudo apt-get install libjson-c-dev`

`sudo apt-get install libmosquittopp-dev`

`sudo apt-get install libssl1.0-dev`

## Build

`make`

## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
-T|REQUIRED|topic|mqtt topic
-H|REQUIRED|qualified host|mqtt host operating mqtt server
-s|OPTIONAL|seconds|sampling interval
-v|OPTIONAL|(none)|sets verbose mode
-h|OPTIONAL|(none)|displays help and exits
--help|OPTIONAL|(none)|displays help and exits
--json-enclosing-array|OPTIONAL|array name. wrap data array


