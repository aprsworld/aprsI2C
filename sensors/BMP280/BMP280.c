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
#include <mosquitto.h>
#include "local.h"

#define DT 0.02         // [s/loop] loop period. 20ms
#define AA 0.97         // complementary filter constant

#define A_GAIN 0.0573    // [deg/LSB]
#define G_GAIN 0.070     // [deg/s/LSB]
#define RAD_TO_DEG 57.29578
#define M_PI 3.14159265358979323846

int outputDebug=0;

static int mqtt_port=1883;
static char mqtt_host[256];
static char mqtt_topic[256];
static struct mosquitto *mosq;

static void _mosquitto_shutdown(void);


/* JSON stuff */
static char jsonEnclosingArray[256];
struct json_object *jobj_enclosing,*jobj,*jobj_sensors;
struct json_object *jobj_sensors_bmp280;
struct json_object *jobj_sensors_LSM9DS1,*jobj_sensors_LSM9DS1_gyro,*jobj_sensors_LSM9DS1_accel,*jobj_sensors_LSM9DS1_magnet;

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
	fprintf(stderr,"-T                       topic          mqtt topic\n");
	fprintf(stderr,"-H                       host           mqtt topic\n");
	fprintf(stderr,"-P                       port           mqtt port\n");
	fprintf(stderr,"-s                       mSeconds       sampling interval 1-1000\n");
	fprintf(stderr,"-v                                      verbose debugging mode\n");
	fprintf(stderr,"-h                                      this message\n");
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
static void bmp280_sample(int i2cHandle, int i2cAddress) {
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

//struct json_object *jobj_sensors_LSM9DS1,*jobj_sensors_LSM9DS1_gyro,*jobj_sensors_LSM9DS1_accel,*jobj_sensors_LSM9DS1_magnet;

void LSM9DS1_init(int i2cHandle, int i2cAddress) {
#if 0
	/* one time sensor initialization */
	int opResult;

	/* address of device we will be working with */
	opResult = ioctl(i2cHandle, I2C_SLAVE, i2cAddress);
#else
void detectIMU(void);
void enableIMU(void);

	detectIMU();
	enableIMU();

#endif

}

void LSM9DS1_sample(int i2cHandle, int i2cAddress) {
#if 0
	/* sample sensor */
	int opResult;

	/* address of device we will be working with */
	opResult = ioctl(i2cHandle, I2C_SLAVE, i2cAddress);
#endif
	char buffer[64];
        float accXnorm,accYnorm,pitch,roll,magXcomp,magYcomp;



	float rate_gyr_y = 0.0;   // [deg/s]
	float rate_gyr_x = 0.0;   // [deg/s]
	float rate_gyr_z = 0.0;   // [deg/s]

	int  accRaw[3];
	int  magRaw[3];
	int  gyrRaw[3];



	float gyroXangle = 0.0;
	float gyroYangle = 0.0;
	float gyroZangle = 0.0;
	float AccYangle = 0.0;
	float AccXangle = 0.0;
	float CFangleX = 0.0;
	float CFangleY = 0.0;


	//read MAG ACC and GYR data
	readACC(accRaw);
	readGYR(gyrRaw);
	readMAG(magRaw);


	//Convert Gyro raw to degrees per second
	rate_gyr_x = (float) gyrRaw[0] * G_GAIN;
	rate_gyr_y = (float) gyrRaw[1]  * G_GAIN;
	rate_gyr_z = (float) gyrRaw[2]  * G_GAIN;



	//Calculate the angles from the gyro
	gyroXangle+=rate_gyr_x*DT;
	gyroYangle+=rate_gyr_y*DT;
	gyroZangle+=rate_gyr_z*DT;




	//Convert Accelerometer values to degrees
	AccXangle = (float) (atan2(accRaw[1],accRaw[2])+M_PI)*RAD_TO_DEG;
	AccYangle = (float) (atan2(accRaw[2],accRaw[0])+M_PI)*RAD_TO_DEG;

	//Change the rotation value of the accelerometer to -/+ 180 and move the Y axis '0' point to up.
	//Two different pieces of code are used depending on how your IMU is mounted.
	//If IMU is upside down
	/*
		if (AccXangle >180)
				AccXangle -= (float)360.0;

		AccYangle-=90;
		if (AccYangle >180)
				AccYangle -= (float)360.0;
	*/

	//If IMU is up the correct way, use these lines
	AccXangle -= (float)180.0;
	if (AccYangle > 90)
			AccYangle -= (float)270;
	else
		AccYangle += (float)90;


	//Complementary filter used to combine the accelerometer and gyro values.
	CFangleX=AA*(CFangleX+rate_gyr_x*DT) +(1 - AA) * AccXangle;
	CFangleY=AA*(CFangleY+rate_gyr_y*DT) +(1 - AA) * AccYangle;


	//printf ("   GyroX  %7.3f \t AccXangle \e[m %7.3f \t \033[22;31mCFangleX %7.3f\033[0m\t GyroY  %7.3f \t AccYangle %7.3f \t \033[22;36mCFangleY %7.3f\t\033[0m\n",gyroXangle,AccXangle,CFangleX,gyroYangle,AccYangle,CFangleY);

	/* put data in JSON objects */
	/* put gyroscope data in jobj_sensors_LSM9DS1_gyro */
	snprintf(buffer,sizeof(buffer),"%1.3f",gyroXangle);
	json_object_object_add(jobj_sensors_LSM9DS1_gyro, "gyro_x", json_object_new_string(buffer));
	snprintf(buffer,sizeof(buffer),"%1.3f",gyroYangle);
	json_object_object_add(jobj_sensors_LSM9DS1_gyro, "gyro_y", json_object_new_string(buffer));

	/* put accelerometer data in jobj_sensors_LSM9DS1_accel */
	snprintf(buffer,sizeof(buffer),"%1.3f",AccXangle);
	json_object_object_add(jobj_sensors_LSM9DS1_accel, "accel_x", json_object_new_string(buffer));
	snprintf(buffer,sizeof(buffer),"%1.3f",AccYangle);
	json_object_object_add(jobj_sensors_LSM9DS1_accel, "accel_y", json_object_new_string(buffer));

	//Compute heading
	float heading = 180 * atan2(magRaw[1],magRaw[0])/M_PI;

	//Convert heading to 0 - 360
	if(heading < 0)
		heading += 360;


	/* put magnetometer data in jobj_sensors_LSM9DS1_magnet */
	snprintf(buffer,sizeof(buffer),"%1.3f",heading);
	json_object_object_add(jobj_sensors_LSM9DS1_magnet, "magnet_heading", json_object_new_string(buffer));


	/* put gyro, accel, and magnet into main LSM9DS1 */
	json_object_object_add(jobj_sensors_LSM9DS1, "gyrometer", jobj_sensors_LSM9DS1_gyro);
	json_object_object_add(jobj_sensors_LSM9DS1, "accelerometer", jobj_sensors_LSM9DS1_accel);
	json_object_object_add(jobj_sensors_LSM9DS1, "magnetometer", jobj_sensors_LSM9DS1_magnet);
}

/*   waits until the milliseond time changes fro before when to after when */
void wait_for_it(int interval ) {
	struct timeval start_time;
	struct tm *now;
	char timestamp[32];
	static int next;
	int when;

	when = next += interval;
	when %= 1000;

	when *= 1000 ;	// convert to micro seconds
	when += 250;
	// printf("when = %d\n",when);	fflush(stdout);


	for ( ;; ) {
		while ( 0 != gettimeofday( &start_time, (struct timezone *) 0))
			;
		if ( when > start_time.tv_usec )
			break;
	}
	for ( ;; ) {
		while ( 0 != gettimeofday( &start_time, (struct timezone *) 0))
			;
		if ( when <= start_time.tv_usec )
			break;
	}
	/* okay we transition from before to after */
}
void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	printf("# connect_callback, rc=%d\n", result);
}
static struct mosquitto *_mosquitto_startup(void) {
	char clientid[24];
	int rc = 0;


	fprintf(stderr,"# mqtt-send-example start-up\n");


	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mqtt-send-example_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		mosquitto_connect_callback_set(mosq, connect_callback);
//		mosquitto_message_callback_set(mosq, message_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
		// if ( 0 != rc )	what do I do?

		/* start mosquitto network handling loop */
		mosquitto_loop_start(mosq);
		}

return	mosq;
}
static void _mosquitto_shutdown(void) {

if ( mosq ) {
	
	/* disconnect mosquitto so we can be done */
	mosquitto_disconnect(mosq);
	/* stop mosquitto network handling loop */
	mosquitto_loop_stop(mosq,0);


	mosquitto_destroy(mosq);
	}

fprintf(stderr,"# mosquitto_lib_cleanup()\n");
mosquitto_lib_cleanup();
}
int BMP280_pub(const char *message ) {
	int rc = 0;

	static int messageID;
	/* instance, message ID pointer, topic, data length, data, qos, retain */
	rc = mosquitto_publish(mosq, &messageID, mqtt_topic, strlen(message), message, 0, 0); 

	if (0 != outputDebug) fprintf(stderr,"# mosquitto_publish provided messageID=%d and return code=%d\n",messageID,rc);

	/* check return status of mosquitto_publish */ 
	/* this really just checks if mosquitto library accepted the message. Not that it was actually send on the network */
	if ( MOSQ_ERR_SUCCESS == rc ) {
		/* successful send */
	} else if ( MOSQ_ERR_INVAL == rc ) {
		fprintf(stderr,"# mosquitto error invalid parameters\n");
	} else if ( MOSQ_ERR_NOMEM == rc ) {
		fprintf(stderr,"# mosquitto error out of memory\n");
	} else if ( MOSQ_ERR_NO_CONN == rc ) {
		fprintf(stderr,"# mosquitto error no connection\n");
	} else if ( MOSQ_ERR_PROTOCOL == rc ) {
		fprintf(stderr,"# mosquitto error protocol\n");
	} else if ( MOSQ_ERR_PAYLOAD_SIZE == rc ) {
		fprintf(stderr,"# mosquitto error payload too large\n");
	} else if ( MOSQ_ERR_MALFORMED_UTF8 == rc ) {
		fprintf(stderr,"# mosquitto error topic is not valid UTF-8\n");
	} else {
		fprintf(stderr,"# mosquitto unknown error = %d\n",rc);
	}


return	rc;
}

int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int LSM9DS1_i2cAddress; 
	int BMP280_i2cAddress; 
	int i2cHandle;
	int opResult = 0;	/* for error checking of operations */
	int samplingInterval = 500;	// milliseconds;

	/* sample loop */
	struct timeval time;
	struct tm *now;
	char timestamp[32];




	fprintf(stderr,"# BMP280 read utility\n");

	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	BMP280_i2cAddress=0x77;		/* default address of BMP280 device is 0x77. It can also be 0x76 */ 	
	LSM9DS1_i2cAddress=0x12;	/* default address of LSM9DS1 device is TODO I DONT KNOW. */ 	

	strcpy(jsonEnclosingArray,"BerryIMU"); 

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"json-enclosing-array",             required_argument, 0, 'j' },
		        {"i2c-device",                       required_argument, 0, 'i' },
		        {"LSM9DS1-i2c-address",              required_argument, 0, 'l' },
		        {"BMP280-i2c-address",               required_argument, 0, 'b' },
		        {"help",                             no_argument,       0, 'h' },
		        {"samplingInterval",                 required_argument, 0, 's' },
		        {0,                                  0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "s:T:H:P:vh", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'T':	
				strncpy(mqtt_topic,optarg,sizeof(mqtt_topic));
				break;
			case 'H':	
				strncpy(mqtt_host,optarg,sizeof(mqtt_host));
				break;
			case 'P':
				mqtt_port = atoi(optarg);
				break;
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
			case 'l':
				sscanf(optarg,"%x",&LSM9DS1_i2cAddress);
				break;
			case 'b':
				sscanf(optarg,"%x",&BMP280_i2cAddress);
				break;
			case 'i':
				strncpy(i2cDevice,optarg,sizeof(i2cDevice)-1);
				i2cDevice[sizeof(i2cDevice)-1]='\0';
				break;
			case 'v':
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
			/* JSON settings */
			case 'j':
				strncpy(jsonEnclosingArray,optarg,sizeof(jsonEnclosingArray)-1);
				jsonEnclosingArray[sizeof(jsonEnclosingArray)-1]='\0';
				break;
		}
	}
	if ( ' ' >= mqtt_host[0] ) { fputs("# <-H mqtt_host>	\n",stderr); exit(1); } else fprintf(stderr,"# mqtt_host=%s\n",mqtt_host);
	if ( ' ' >= mqtt_topic[0] ) { fputs("# <-T mqtt_topic>	\n",stderr); exit(1); } else fprintf(stderr,"# mqtt_topic=%s\n",mqtt_topic);

	if ( 0 == _mosquitto_startup() )
		return	1;


	/* start-up verbosity */
	fprintf(stderr,"# using I2C device %s\n",i2cDevice);
	fprintf(stderr,"# using BMP280 I2C device address of 0x%02X\n",BMP280_i2cAddress);
	fprintf(stderr,"# using LSM9DS1 I2C device address of 0x%02X\n",LSM9DS1_i2cAddress);


	/* Open I2C bus */
	i2cHandle = open(i2cDevice, O_RDWR);

	if ( -1 == i2cHandle ) {
		fprintf(stderr,"# Error opening I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	/* not using 10 bit addresses */
	opResult = ioctl(i2cHandle, I2C_TENBIT, 0);



	fprintf(stderr,"# samplingInterval = %d mSeconds\n",samplingInterval);

	/* I2C running, now initialize / configure hardware */
	fprintf(stderr,"# initializing and configuring BMP280 ...");
	bmp280_init(i2cHandle,BMP280_i2cAddress);
	fprintf(stderr,"done\n");

	fprintf(stderr,"# initializing and configuring LSM9DS1 ...");
	LSM9DS1_init(i2cHandle,LSM9DS1_i2cAddress);
	fprintf(stderr,"done\n");


	/* allow hardware to finish initializing. May not be nescessary. */
	fprintf(stderr,"# waiting to start\n");
	sleep(1);


	/* ready to periodically sample */
	fprintf(stderr,"# starting sample loop\n");
	int	rc = 0;
	while ( 0 == rc ) {
		/* setup JSON objects */
		jobj_enclosing = json_object_new_object();
		jobj = json_object_new_object();
		jobj_sensors = json_object_new_object();
		jobj_sensors_bmp280 = json_object_new_object();
		jobj_sensors_LSM9DS1 = json_object_new_object();
		jobj_sensors_LSM9DS1_gyro = json_object_new_object();
		jobj_sensors_LSM9DS1_accel = json_object_new_object();
		jobj_sensors_LSM9DS1_magnet = json_object_new_object();

		wait_for_it(samplingInterval);


		/* timestamp of start of samples */
		gettimeofday(&time, NULL); 
		now = localtime(&time.tv_sec);


		/* sample sensors */
		bmp280_sample(i2cHandle,BMP280_i2cAddress);
		LSM9DS1_sample(i2cHandle,LSM9DS1_i2cAddress);


		/* pack data into JSON objects */
	        if ( 0 == now ) {
        	        fprintf(stderr,"# error calling localtime() %s",strerror(errno));
	                exit(1);
	        }
	        snprintf(timestamp,sizeof(timestamp),"%04d-%02d-%02d %02d:%02d:%02d.%03ld", 
			1900 + now->tm_year,
			1 + now->tm_mon, 
			now->tm_mday,
			now->tm_hour,
			now->tm_min,
			now->tm_sec,
			time.tv_usec/1000
		);
		json_object_object_add(jobj,"date",json_object_new_string(timestamp));


		/* add individual sensors to sensor object */
		json_object_object_add(jobj_sensors, "bmp280", jobj_sensors_bmp280);
		json_object_object_add(jobj_sensors, "LSM9DS1", jobj_sensors_LSM9DS1);

		/* add sensors to main JSON object */
		json_object_object_add(jobj, "sensors", jobj_sensors);

		/* enclose array */
		json_object_object_add(jobj_enclosing, jsonEnclosingArray, jobj);


		char	*s = json_object_to_json_string_ext(jobj_enclosing, JSON_C_TO_STRING_PRETTY);
		printf("%s\n", s);

		rc =  BMP280_pub(s);
		/* release JSON object */
		json_object_put(jobj_sensors_bmp280);
		json_object_put(jobj_sensors);
		json_object_put(jobj);
		json_object_put(jobj_enclosing);

	}

	/* close I2C */
	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	_mosquitto_shutdown();
	fprintf(stderr,"# Done...\n");
	return	0;
}
