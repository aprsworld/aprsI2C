/* 
Bosch BMP280 pressure and temperature I2C sensor.

originally alogirithm implemented by / from:
https://github.com/LanderU/BMP280/blob/master/BMP280.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <json.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>


void printUsage(void) {
	fprintf(stderr,"Usage:\n\n");
	fprintf(stderr,"switch           argument       description\n");
	fprintf(stderr,"========================================================================================================\n");
	fprintf(stderr,"--i2c-device             device         /dev/ entry for I2C-dev device\n");
	fprintf(stderr,"--i2c-address            chip address   hex address of chip\n");
	fprintf(stderr,"--json-enclosing-array   array name     wrap data array\n");
	fprintf(stderr,"--help                                  this message\n");
}

int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */
	int i2cHandle;
	int opResult = 0;	/* for error checking of operations */

	/* JSON stuff */
	struct json_object *jobj,*jobj_data;
	char jsonEnclosingArray[256];


	fprintf(stderr,"# BMP280 read utility\n");

	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x77;		/* default address of BMP280 device is 0x77. It can also be 0x76 */ 	

	strcpy(jsonEnclosingArray,"BMP280"); 

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"json-enclosing-array",             required_argument, 0, 'j' },
		        {"i2c-device",                       required_argument, 0, 'i' },
		        {"i2c-address",                      required_argument, 0, 'a' },
		        {"help",                             no_argument,       0, 'h' },
		        {0,                                  0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			/* getopt / standard program */
			case '?':
				/* getopt error of missing argument or unknown option */
				exit(1);
			case 'h':
				printUsage();
				exit(0);	
				break;
			/* I2C settings */
			case 'a':
				sscanf(optarg,"%x",&i2cAddress);
				break;
			case 'i':
				strncpy(i2cDevice,optarg,sizeof(i2cDevice)-1);
				i2cDevice[sizeof(i2cDevice)-1]='\0';
				break;
			/* JSON settings */
			case 'j':
				strncpy(jsonEnclosingArray,optarg,sizeof(jsonEnclosingArray)-1);
				jsonEnclosingArray[sizeof(jsonEnclosingArray)-1]='\0';
				break;
		}
	}

	/* start-up verbosity */
	fprintf(stderr,"# using I2C device %s\n",i2cDevice);
	fprintf(stderr,"# using I2C device address of 0x%02X\n",i2cAddress);


	/* Open I2C bus */
	i2cHandle = open(i2cDevice, O_RDWR);

	if ( -1 == i2cHandle ) {
		fprintf(stderr,"# Error opening I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	/* not using 10 bit addresses */
	opResult = ioctl(i2cHandle, I2C_TENBIT, 0);
	/* address of device we will be working with */
	opResult = ioctl(i2cHandle, I2C_SLAVE, i2cAddress);



//int BMP280_make_int(

	// Read 24 bytes of data from address(0x88)
	uint8_t reg[1] = {0x88};
	write(i2cHandle, reg, 1);
	uint8_t data[24] = {0};
	if(read(i2cHandle, data, 24) != 24) {
		printf("Error : Input/output Error \n");
		exit(1);
	}
	// Convert the data
	// temp coefficents
	int dig_T1 = data[1] * 256 + data[0];
	int dig_T2 = data[3] * 256 + data[2];
	if(dig_T2 > 32767) {
		dig_T2 -= 65536;
	}
	int dig_T3 = data[5] * 256 + data[4];
	if(dig_T3 > 32767) {
		dig_T3 -= 65536;
	}
	// pressure coefficents
	int dig_P1 = data[7] * 256 + data[6];
	int dig_P2  = data[9] * 256 + data[8];
	if(dig_P2 > 32767) {
		dig_P2 -= 65536;
	}
	int dig_P3 = data[11]* 256 + data[10];
	if(dig_P3 > 32767) {
		dig_P3 -= 65536;
	}
	int dig_P4 = data[13]* 256 + data[12];
	if(dig_P4 > 32767) {
		dig_P4 -= 65536;
	}
	int dig_P5 = data[15]* 256 + data[14];
	if(dig_P5 > 32767) {
		dig_P5 -= 65536;
	}
	int dig_P6 = data[17]* 256 + data[16];
	if(dig_P6 > 32767) {
		dig_P6 -= 65536;
	}
	int dig_P7 = data[19]* 256 + data[18];
	if(dig_P7 > 32767) {
		dig_P7 -= 65536;
	}
	int dig_P8 = data[21]* 256 + data[20];
	if(dig_P8 > 32767) {
		dig_P8 -= 65536;
	}
	int dig_P9 = data[23]* 256 + data[22];
	if(dig_P9 > 32767) {
		dig_P9 -= 65536;
	}
		
	// Select control measurement register(0xF4)
	// Normal mode, temp and pressure over sampling rate = 1(0x27)
	uint8_t config[2] = {0};
	config[0] = 0xF4;
	config[1] = 0x27;
	write(i2cHandle, config, 2);
	
	// Select config register(0xF5)
	// Stand_by time = 1000 ms(0xA0)
	config[0] = 0xF5;
	config[1] = 0xA0;
	write(i2cHandle, config, 2);
	sleep(1);
	
	// Read 8 bytes of data from register(0xF7)
	// pressure msb1, pressure msb, pressure lsb, temp msb1, temp msb, temp lsb, humidity lsb, humidity msb
	reg[0] = 0xF7;
	write(i2cHandle, reg, 1);
	if(read(i2cHandle, data, 8) != 8) {
		printf("Error : Input/output Error \n");
		exit(1);
	}
	
	// Convert pressure and temperature data to 19-bits
	long adc_p = (((long)data[0] * 65536) + ((long)data[1] * 256) + (long)(data[2] & 0xF0)) / 16;
	long adc_t = (((long)data[3] * 65536) + ((long)data[4] * 256) + (long)(data[5] & 0xF0)) / 16;
		
	// Temperature offset calculations
	double var1 = (((double)adc_t) / 16384.0 - ((double)dig_T1) / 1024.0) * ((double)dig_T2);
	double var2 = ((((double)adc_t) / 131072.0 - ((double)dig_T1) / 8192.0) *(((double)adc_t)/131072.0 - ((double)dig_T1)/8192.0)) * ((double)dig_T3);
	double t_fine = (long)(var1 + var2);
	double temperatureC = (var1 + var2) / 5120.0;
		
	// Pressure offset calculations
	var1 = ((double)t_fine / 2.0) - 64000.0;
	var2 = var1 * var1 * ((double)dig_P6) / 32768.0;
	var2 = var2 + var1 * ((double)dig_P5) * 2.0;
	var2 = (var2 / 4.0) + (((double)dig_P4) * 65536.0);
	var1 = (((double) dig_P3) * var1 * var1 / 524288.0 + ((double) dig_P2) * var1) / 524288.0;
	var1 = (1.0 + var1 / 32768.0) * ((double)dig_P1);
	double p = 1048576.0 - (double)adc_p;
	p = (p - (var2 / 4096.0)) * 6250.0 / var1;
	var1 = ((double) dig_P9) * p * p / 2147483648.0;
	var2 = p * ((double) dig_P8) / 32768.0;
	double pressureHPA = (p + (var1 + var2 + ((double)dig_P7)) / 16.0) / 100;
	
	/* setup JSON objects */
	jobj = json_object_new_object();
	jobj_data = json_object_new_object();

	/* put data in JSON */
	json_object_object_add(jobj_data, "pressure_HPA", json_object_new_double(pressureHPA));
	json_object_object_add(jobj_data, "temperature_C", json_object_new_double(temperatureC));

	json_object_object_add(jobj, jsonEnclosingArray, jobj_data);


	if ( strlen(jsonEnclosingArray) > 0 ) {
		printf("%s\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY));
	} else {
		printf("%s\n", json_object_to_json_string_ext(jobj_data, JSON_C_TO_STRING_PRETTY));
	}

	/* release JSON object */
	json_object_put(jobj_data);
	json_object_put(jobj);

	/* close I2C */
	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");

}
