CC=gcc
CFLAGS=-I.

### JJJ compiling with:
# gcc imuToMQTT.c sensor_BMP280.c sensor_LSM9DS1.c -o imuToMQTT -I. -I/usr/include/json-c/ -lm -ljson-c -lmosquitto

imuToMQTT: imuToMQTT.c sensor_BMP280.c sensor_LSM9DS1.c  \
	LSM9DS0.h  LSM9DS1.h  i2c-dev.h  sensor_BMP280.h  sensor_LSM9DS1.h
	$(CC) imuToMQTT.c sensor_BMP280.c sensor_LSM9DS1.c -g -o imuToMQTT -I. -I/usr/include/json-c/ -lm -ljson-c -lmosquitto

