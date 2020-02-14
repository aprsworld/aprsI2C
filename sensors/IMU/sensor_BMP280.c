/* 
Bosch BMP280 pressure and temperature I2C sensor.
*/

#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <json.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

struct json_object *jobj_sensors_bmp280;

/* BMP280 calibration values that are read once on initialization */
typedef struct { 
	int dig_T1, dig_T2, dig_T3;
	int dig_P1, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_struct_bmp280;

bmp280_struct_bmp280 bmp280;


int bmp280_make_int(uint8_t msb, uint8_t lsb, int sign_extend) {
	int i;

	i = msb * 256 + lsb;

	if ( sign_extend && i > 32767 ) {
		i -= 65536;
	}

	return i;
}

void bmp280_init(int i2cHandle, int i2cAddress) {
	int opResult;
	uint8_t reg[1];
	uint8_t data[24];

	/* address of device we will be working with */
	opResult = ioctl(i2cHandle, I2C_SLAVE, i2cAddress);

	// Read 24 bytes of data from address(0x88)
	reg[0] = 0x88;
	opResult = write(i2cHandle, reg, 1);
	if (opResult != 1) {
		fprintf(stderr,"# I2C write error in address set. No ACK! Exiting...\n");
		exit(2);
	}


	if ( read(i2cHandle, data, 24) != 24 ) {
		fprintf(stderr,"# I2C read error. Exiting...\n");
		exit(1);
	}

	/* temperature */
	bmp280.dig_T1 = bmp280_make_int(data[1],data[0],0); /* unsigned */
	bmp280.dig_T2 = bmp280_make_int(data[3],data[2],1); /* signed */
	bmp280.dig_T3 = bmp280_make_int(data[5],data[4],1); 
	
	/* pressure */
	bmp280.dig_P1 = bmp280_make_int(data[7],data[6],0); 
	bmp280.dig_P2 = bmp280_make_int(data[9],data[8],1); 
	bmp280.dig_P3 = bmp280_make_int(data[11],data[10],1); 
	bmp280.dig_P4 = bmp280_make_int(data[13],data[12],1); 
	bmp280.dig_P5 = bmp280_make_int(data[15],data[14],1); 
	bmp280.dig_P6 = bmp280_make_int(data[17],data[16],1); 
	bmp280.dig_P7 = bmp280_make_int(data[19],data[18],1); 
	bmp280.dig_P8 = bmp280_make_int(data[21],data[20],1); 
	bmp280.dig_P9 = bmp280_make_int(data[23],data[22],1); 

		
	// Select control measurement register(0xF4)
	// Normal mode, temp and pressure over sampling rate = 1(0x27)
	uint8_t config[2] = {0};
	config[0] = 0xF4;
	config[1] = 0x27;
	opResult = write(i2cHandle, config, 2);
	if (opResult != 2) {
		fprintf(stderr,"# I2C write error in setting mode. No ACK! Exiting...\n");
		exit(2);
	}
	
	// Select config register(0xF5)
	// Stand_by time = 1000 ms(0xA0)
	config[0] = 0xF5;
	config[1] = 0xA0;
	opResult = write(i2cHandle, config, 2);
	if (opResult != 2) {
		fprintf(stderr,"# I2C write error in setting standby time. No ACK! Exiting...\n");
		exit(2);
	}
	

}


/* read bmp280 device that has been previously configured */
void bmp280_sample(int i2cHandle, int i2cAddress) {
	int opResult;
	uint8_t reg[1];
	uint8_t data[8];

	/* address of device we will be working with */
	opResult = ioctl(i2cHandle, I2C_SLAVE, i2cAddress);


	// Read 8 bytes of data from register(0xF7)
	// pressure msb1, pressure msb, pressure lsb, temp msb1, temp msb, temp lsb, humidity lsb, humidity msb
	reg[0] = 0xF7;
	opResult = write(i2cHandle, reg, 1);
	if (opResult != 1) {
		fprintf(stderr,"# I2C write error. No ACK! Exiting...\n");
		exit(2);
	}
	if ( read(i2cHandle, data, 8) != 8 ) {
		fprintf(stderr, "# I2C read error. Exiting...\n");
		exit(1);
	}
	
	// Convert pressure and temperature data to 19-bits
	long adc_p = (((long)data[0] * 65536) + ((long)data[1] * 256) + (long)(data[2] & 0xF0)) / 16;
	long adc_t = (((long)data[3] * 65536) + ((long)data[4] * 256) + (long)(data[5] & 0xF0)) / 16;
		
	// Temperature offset calculations
	double var1 = (((double)adc_t) / 16384.0 - ((double)bmp280.dig_T1) / 1024.0) * ((double)bmp280.dig_T2);
	double var2 = ((((double)adc_t) / 131072.0 - ((double)bmp280.dig_T1) / 8192.0) *(((double)adc_t)/131072.0 - ((double)bmp280.dig_T1)/8192.0)) * ((double)bmp280.dig_T3);
	double t_fine = (long)(var1 + var2);
	double temperatureC = (var1 + var2) / 5120.0;
		
	// Pressure offset calculations
	var1 = ((double)t_fine / 2.0) - 64000.0;
	var2 = var1 * var1 * ((double)bmp280.dig_P6) / 32768.0;
	var2 = var2 + var1 * ((double)bmp280.dig_P5) * 2.0;
	var2 = (var2 / 4.0) + (((double)bmp280.dig_P4) * 65536.0);
	var1 = (((double) bmp280.dig_P3) * var1 * var1 / 524288.0 + ((double) bmp280.dig_P2) * var1) / 524288.0;
	var1 = (1.0 + var1 / 32768.0) * ((double)bmp280.dig_P1);
	double p = 1048576.0 - (double)adc_p;
	p = (p - (var2 / 4096.0)) * 6250.0 / var1;
	var1 = ((double) bmp280.dig_P9) * p * p / 2147483648.0;
	var2 = p * ((double) bmp280.dig_P8) / 32768.0;
	double pressureHPA = (p + (var1 + var2 + ((double)bmp280.dig_P7)) / 16.0) / 100;
	
	json_object_object_add(jobj_sensors_bmp280, "pressure_HPA", json_object_new_double(pressureHPA));
	json_object_object_add(jobj_sensors_bmp280, "temperature_C", json_object_new_double(temperatureC));
}

