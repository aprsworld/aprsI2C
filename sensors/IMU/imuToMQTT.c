/* 
send IMU data to MQTT
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
#include "sensor_BMP280.h"
#include "sensor_LSM9DS1.h"

int outputDebug=0;

static int mqtt_port=1883;
static char mqtt_host[256];
static char mqtt_topic[256];
static struct mosquitto *mosq;

static void _mosquitto_shutdown(void);


/* JSON stuff */
static char jsonEnclosingArray[256];
struct json_object *jobj_enclosing,*jobj,*jobj_sensors;

/* sensor JSON objects come in from sensor_whatever.c files */
extern struct json_object *jobj_sensors_bmp280;
extern struct json_object *jobj_sensors_LSM9DS1,*jobj_sensors_LSM9DS1_gyro,*jobj_sensors_LSM9DS1_accel,*jobj_sensors_LSM9DS1_magnet;


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


	fprintf(stderr,"# _mosquitto_startup()\n");

	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "mqtt-send-example_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		mosquitto_connect_callback_set(mosq, connect_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		/* start mosquitto network handling loop */
		mosquitto_loop_start(mosq);
	}
	return	mosq;
}

static void _mosquitto_shutdown(void) {
	fprintf(stderr,"# _mosquitto_shutdown()\n");

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

int m_pub(const char *message ) {
	int rc = 0;

	static int messageID;
	/* instance, message ID pointer, topic, data length, data, qos, retain */
	rc = mosquitto_publish(mosq, &messageID, mqtt_topic, strlen(message), message, 0, 0); 

	if (0 != outputDebug) { 
		fprintf(stderr,"# mosquitto_publish provided messageID=%d and return code=%d\n",messageID,rc);
	}

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
	/* check MQTT arguments */
	if ( ' ' >= mqtt_host[0] ) { 
		fputs("# <-H mqtt_host>	\n",stderr); 
		exit(1); 
	} else {
		fprintf(stderr,"# mqtt_host=%s\n",mqtt_host);
	}

	if ( ' ' >= mqtt_topic[0] ) { 
		fputs("# <-T mqtt_topic>\n",stderr); 
		exit(1); 
	} else { 
		fprintf(stderr,"# mqtt_topic=%s\n",mqtt_topic);
	}


	/* attempt to start mosquitto */
	if ( 0 == _mosquitto_startup() ) {
		return	1;
	}


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
	fprintf(stderr,"# initializing and configuring ... ");

	fprintf(stderr,"# BMP280 ... ");
	bmp280_init(i2cHandle,BMP280_i2cAddress);
	fprintf(stderr,"# BMP280 initialized\n");

	fprintf(stderr,"# LSM9DS1 ... ");
	LSM9DS1_init(i2cHandle,LSM9DS1_i2cAddress);
	fprintf(stderr,"# LSM9DS1 done\n");


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


		/* convert array to string */
		char	*s = (char *) json_object_to_json_string_ext(jobj_enclosing, JSON_C_TO_STRING_PRETTY);
		printf("%s\n", s);

		/* send to MQTT */
		rc =  m_pub(s);


		/* release JSON object */
		json_object_put(jobj_sensors_LSM9DS1);
		json_object_put(jobj_sensors_LSM9DS1_gyro);
		json_object_put(jobj_sensors_LSM9DS1_accel);
		json_object_put(jobj_sensors_LSM9DS1_magnet);
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
	
	/* shut down MQTT */
	_mosquitto_shutdown();

	fprintf(stderr,"# Done...\n");
	return	0;
}
