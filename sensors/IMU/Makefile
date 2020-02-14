CC=gcc
CFLAGS=-I.

### JJJ compiling with:
# gcc imuToMQTT.c sensor_BMP280.c sensor_LSM9DS1.c -o imuToMQTT -I. -I/usr/include/json-c/ -lm -ljson-c -lmosquitto

imuToMQTT: imuToMQTT.c sensor_BMP280.c sensor_LSM9DS1.c 
	$(CC) imuToMQTT.c sensor_BMP280.c sensor_LSM9DS1.c -g -o imuToMQTT -I. -I/usr/include/json-c/ -lm -ljson-c -lmosquitto

