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
#include <sys/time.h>
#include <time.h>

/* JSON stuff */
static char jsonEnclosingArray[256];
struct json_object *jobj_enclosing,*jobj,*jobj_sensors;
struct json_object *jobj_sensors_bmp280;

/* BMP280 calibration values that are read once on initialization */
typedef struct { 
	int dig_T1, dig_T2, dig_T3;
	int dig_P1, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_struct_bmp280;

bmp280_struct_bmp280 bmp280;


void printUsage(void) {
	fprintf(stderr,"Usage:\n\n");
	fprintf(stderr,"switch           argument       description\n");
	fprintf(stderr,"========================================================================================================\n");
	fprintf(stderr,"--i2c-device             device         /dev/ entry for I2C-dev device\n");
	fprintf(stderr,"--i2c-address            chip address   hex address of chip\n");
	fprintf(stderr,"--json-enclosing-array   array name     wrap data array\n");
	fprintf(stderr,"--help                                  this message\n");
}
static uint64_t microtime(void) {
	struct timeval time;
	gettimeofday(&time, NULL); 
	return ((uint64_t)time.tv_sec * 1000000) + time.tv_usec;
}


int bmp280_make_int(uint8_t msb, uint8_t lsb, int sign_extend) {
	int i;

	i = msb * 256 + lsb;

	if ( sign_extend && i > 32767 ) {
		i -= 65536;
	}

	return i;
}

void bmp280_init(int i2cHandle) {
	int opResult;
	uint8_t reg[1];
	uint8_t data[24];

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
static void bmp280_sample(int i2cHandle) {
	int opResult;
	uint8_t reg[1];
	uint8_t data[8];


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

int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */
	int i2cHandle;
	int opResult = 0;	/* for error checking of operations */
	int samplingInterval = 500;	// milliseconds;

	/* sample loop */
	struct timeval time;
	struct tm *now;
	char timestamp[32];




	fprintf(stderr,"# BMP280 read utility\n");

	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x77;		/* default address of BMP280 device is 0x77. It can also be 0x76 */ 	

	strcpy(jsonEnclosingArray,"BerryIMU"); 

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"json-enclosing-array",             required_argument, 0, 'j' },
		        {"i2c-device",                       required_argument, 0, 'i' },
		        {"i2c-address",                      required_argument, 0, 'a' },
		        {"help",                             no_argument,       0, 'h' },
		        {"samplingInterval",                 required_argument, 0, 's' },
		        {0,                                  0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "s:", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 's':
				samplingInterval = atoi(optarg);
				break;
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



	fprintf(stderr,"# samplingInterval = %d mSeconds\n",samplingInterval);

	/* I2C running, now initialize / configure hardware */
	fprintf(stderr,"# initializing and configuring BMP280 ...");
	bmp280_init(i2cHandle);
	fprintf(stderr,"done\n");


	/* allow hardware to finish initializing. May not be nescessary. */
	fprintf(stderr,"# waiting to start\n");
	sleep(1);


	/* ready to periodically sample */
	fprintf(stderr,"# starting sample loop\n");
	while ( 1 ) {
		/* setup JSON objects */
		jobj_enclosing = json_object_new_object();
		jobj = json_object_new_object();
		jobj_sensors = json_object_new_object();
		jobj_sensors_bmp280 = json_object_new_object();

		/* timestamp of start of samples */
		gettimeofday(&time, NULL); 
		now = localtime(&time.tv_sec);


		/* sample sensors */
		bmp280_sample(i2cHandle);


		/* pack data into JSON objects */
	        if ( 0 == now ) {
        	        fprintf(stderr,"# error calling localtime() %s",strerror(errno));
	                exit(1);
	        }
	        snprintf(timestamp,sizeof(timestamp),"%04d-%02d-%02d %02d:%02d:%02d.%03ld", 1900 + now->tm_year,1 + now->tm_mon, now->tm_mday,now->tm_hour,now->tm_min,now->tm_sec,time.tv_usec/1000);
		json_object_object_add(jobj,"date",json_object_new_string(timestamp));


		/* add individual sensors to sensor object */
		json_object_object_add(jobj_sensors, "bmp280", jobj_sensors_bmp280);

		/* add sensors to main JSON object */
		json_object_object_add(jobj, "sensors", jobj_sensors);

		/* enclose array */
		json_object_object_add(jobj_enclosing, jsonEnclosingArray, jobj);


//		if ( strlen(jsonEnclosingArray) > 0 ) {
			printf("%s\n", json_object_to_json_string_ext(jobj_enclosing, JSON_C_TO_STRING_PRETTY));
//		} else {
//			printf("%s\n", json_object_to_json_string_ext(jobj_sensors_bmp280, JSON_C_TO_STRING_PRETTY));
//		}

		/* release JSON object */
		json_object_put(jobj_sensors_bmp280);
		json_object_put(jobj_sensors);
		json_object_put(jobj);
		json_object_put(jobj_enclosing);

		/* TODO: more periodic sampling */
		/* TODO: possible solution discussed at https://qnaplus.com/implement-periodic-timer-linux/ */
		usleep(1000*samplingInterval);
	}

	/* close I2C */
	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");

}
