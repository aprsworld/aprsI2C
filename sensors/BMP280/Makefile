CC=gcc
CFLAGS=-I.

BMP280: BMP280.c gyro_accelerometer_IMU.o local.h
	$(CC) BMP280.c -g -o BMP280 gyro_accelerometer_IMU.o -I. -I/usr/include/json-c/ -lm -ljson-c -lmosquitto

gyro_accelerometer_IMU.o: gyro_accelerometer_IMU.c
	 $(CC) -g -c gyro_accelerometer_IMU.c 
