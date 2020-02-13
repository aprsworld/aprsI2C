# BMP280

Read the BerryIMU for gyro, accelerometer and magnometer.   Processes the atmospheric temperature and pressure.

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


void printUsage(void) {
        fprintf(stderr,"Usage:\n\n");
        fprintf(stderr,"switch           argument       description\n");
        fprintf(stderr,"========================================================================================================\n");
        fprintf(stderr,"--i2c-device             device         /dev/ entry for I2C-dev device\n");
        fprintf(stderr,"--i2c-address            chip address   hex address of chip\n");
        fprintf(stderr,"--json-enclosing-array   array name     wrap data array\n");
        fprintf(stderr,"-T                       topic          mqtt topic\n");
        fprintf(stderr,"-H                       host           mqtt topic\n");
        fprintf(stderr,"-P                       port           mqtt port\n");
        fprintf(stderr,"-s                       mSeconds       sampling interval 1-1000\n");
        fprintf(stderr,"-v                                      verbose debugging mode\n");
        fprintf(stderr,"-h                                      this message\n");
        fprintf(stderr,"--help                                  this message\n");
}

