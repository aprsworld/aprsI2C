#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <arpa/inet.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <json.h>
#include <mosquitto.h>


#include "pzPowerI2C_registers.h"
 
extern char *optarg;
extern int optind, opterr, optopt;

/* processed data read from pzPowerI2C at start */
struct json_object *jobj_enclosing,*jobj, *jobj_data, *jobj_power_off_flags, *jobj_configuration;

/* number of acknowledgement cycles to poll on write for */
#define TIMEOUT_NTRIES 50

/* maximum number of registers on pzPower I2C slave. Each register is 2 bytes */
#define CAPACITY_REGISTERS 64

int outputDebug=0;

static struct mosquitto *mosq;

/* actions to take */
typedef struct {
	/* MQTT */
	int mqtt;
	int mqtt_port;
	char mqtt_host[256];
	char mqtt_topic[256];


	/* program flow */
	int reRead;

	int readLoop;
	int readLoop_value;

	/* 300 series */
	int read;
	int readSwitch;
	int resetSwitchLatch;
	int resetWriteWatchdog;

	/* 400 series */
	int setCommandOff;
	double setCommandOff_value;

	int setCommandOffHoldTime;
	int setCommandOffHoldTime_value;

	int disableReadWatchdog;

	int setReadWatchdogOffThreshold;
	int setReadWatchdogOffThreshold_value;

	int setReadWatchdogOffHoldTime;
	int setReadWatchdogOffHoldTime_value;

	int disableWriteWatchdog;

	int setWriteWatchdogOffThreshold;
	int setWriteWatchdogOffThreshold_value;

	int setWriteWatchdogOffHoldTime;
	int setWriteWatchdogOffHoldTime_value;

	/* 500 series */
	int disableLVD;
	
	int setLVDOffThreshold;
	double setLVDOffThreshold_value;

	int setLVDOffDelay;
	int setLVDOffDelay_value;
	
	int setLVDOnThreshold;
	double setLVDOnThreshold_value;

	int disableHVD;
	
	int setHVDOffThreshold;
	double setHVDOffThreshold_value;

	int setHVDOffDelay;
	int setHVDOffDelay_value;
	
	int setHVDOnThreshold;
	double setHVDOnThreshold_value;

	/* 10000 series */
	int param;
	int param_value;

	int setSerial;
	uint8_t setSerial_prefix;
	uint16_t setSerial_number;

	int setAdcTicks;
	int setAdcTicks_value;

	int setStartupPowerOnDelay;
	int setStartupPowerOnDelay_value;
} struct_action;

/* global structures */
struct_action action={0};

void write_word(int i2cHandle, uint8_t address, uint16_t value) {
	uint8_t txBuffer[3];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */

	/* build tx buffer to send */
	/* address */
	txBuffer[0] = address;
	/* value */
	txBuffer[1]=(value >> 8) & 0xff;
	txBuffer[2]=value & 0xff;

	fprintf(stderr,"# write_word %d to register %d\n",value,address);

	/* write */
	opResult = write(i2cHandle, txBuffer, sizeof(txBuffer));
	if ( -1 == opResult ) {
		fprintf(stderr,"# Error writing value %d to address %d. %s\n",value,address,strerror(errno));

		exit(1);
	}
}


double ntcThermistor(double voltage, double beta, double beta25, double rSource, double vSource) {
	double rt, tKelvin;

	rt = (voltage*rSource)/(vSource-voltage);

	tKelvin=1.0/( (1.0/beta) * log(rt/beta25) + 1.0/298.15);

	return tKelvin-273.15;
}


double adcToVoltage(int adc) {
	double d;

	/* TODO use calibration value */

	d = (40.0 / 1024.0) * adc;

	return d;
}

int voltageToAdc(double voltage) {
	/* TODO use calibration value */

	return round( voltage / (40.0 / 1024.0) );
}

json_object *json_object_new_dateTime(void) {
	struct tm *now;
	struct timeval time;
	char timestamp[32];
        gettimeofday(&time, NULL);
	now = localtime(&time.tv_sec);
	if ( 0 == now ) {
		fprintf(stderr,"# error calling localtime() %s",strerror(errno));
		exit(1);
	}

	snprintf(timestamp,sizeof(timestamp),"%04d-%02d-%02d %02d:%02d:%02d.%03ld",
		1900 + now->tm_year,1 + now->tm_mon, now->tm_mday,now->tm_hour,now->tm_min,now->tm_sec,time.tv_usec/1000);

	
	return	json_object_new_string(timestamp);

}

void decodeRegisters(uint16_t *rxBuffer) {
	int i;
 
	/*
	 * The following create an object and add the question and answer to it.
	 */
	jobj = json_object_new_object();
	jobj_data = json_object_new_object();
	jobj_power_off_flags = json_object_new_object();

	jobj_configuration = json_object_new_object();


	/* data */
	json_object_object_add(jobj,"dateTime",json_object_new_dateTime());

	/* input voltage */
	json_object_object_add(jobj_data, "voltage_in_now",json_object_new_double(
		adcToVoltage(rxBuffer[PZP_I2C_REG_VOLTAGE_INPUT_NOW])
	));

	json_object_object_add(jobj_data, "voltage_in_average",json_object_new_double(
		adcToVoltage(rxBuffer[PZP_I2C_REG_VOLTAGE_INPUT_AVG])
	));

	/* temperature of board from onboard thermistor */
	double t;
	t=ntcThermistor( rxBuffer[PZP_I2C_REG_TEMPERATURE_BOARD_NOW], 3977, 10000, 10000, 1024);
	json_object_object_add(jobj_data, "temperature_pcb_now", json_object_new_double(t));

	t=ntcThermistor( rxBuffer[PZP_I2C_REG_TEMPERATURE_BOARD_AVG], 3977, 10000, 10000, 1024);
	json_object_object_add(jobj_data, "temperature_pcb_average", json_object_new_double(t));

	/* magnetic switch */
	json_object_object_add(jobj_data,"magnetic_switch_state", json_object_new_boolean(rxBuffer[PZP_I2C_REG_SWITCH_MAGNET_NOW]));
	json_object_object_add(jobj_data,"magnetic_switch_latch", json_object_new_boolean(rxBuffer[PZP_I2C_REG_SWITCH_MAGNET_LATCH]));

	/* status */
	json_object_object_add(jobj_data,"sequence_number",           json_object_new_int( rxBuffer[PZP_I2C_REG_SEQUENCE_NUMBER] ) );
	json_object_object_add(jobj_data,"interval_milliseconds",     json_object_new_int( rxBuffer[PZP_I2C_REG_TIME_INTERVAL_MILLISECONDS] ) );
	json_object_object_add(jobj_data,"uptime_minutes",            json_object_new_int( rxBuffer[PZP_I2C_REG_TIME_UPTIME_MINUTES] ) );
	json_object_object_add(jobj_data,"read_watchdog_seconds",     json_object_new_int( rxBuffer[PZP_I2C_REG_TIME_WATCHDOG_READ_SECONDS] ) );
	json_object_object_add(jobj_data,"write_watchdog_seconds",    json_object_new_int( rxBuffer[PZP_I2C_REG_TIME_WATCHDOG_WRITE_SECONDS] ) );
	json_object_object_add(jobj_data,"default_paramaters_written",json_object_new_int( rxBuffer[PZP_I2C_REG_DEFAULT_PARAMS_WRITTEN] ) );
	json_object_object_add(jobj_data,"command_off_seconds",       json_object_new_int( rxBuffer[PZP_I2C_REG_COMMAND_OFF] ) );

	/* put power off flags in their own sub array */
	json_object_object_add(jobj_power_off_flags,"value",          json_object_new_int( rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS] ) );
	json_object_object_add(jobj_power_off_flags,"command",        json_object_new_boolean(rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS]&1));
	json_object_object_add(jobj_power_off_flags,"read_watchdog",  json_object_new_boolean(rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS]&2));
	json_object_object_add(jobj_power_off_flags,"write_watchdog", json_object_new_boolean(rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS]&4));
	json_object_object_add(jobj_power_off_flags,"lvd",            json_object_new_boolean(rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS]&8));
	json_object_object_add(jobj_power_off_flags,"hvd",            json_object_new_boolean(rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS]&16));
	json_object_object_add(jobj_power_off_flags,"ltd",            json_object_new_boolean(rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS]&32));
	json_object_object_add(jobj_power_off_flags,"htd",            json_object_new_boolean(rxBuffer[PZP_I2C_REG_POWER_OFF_FLAGS]&64));
	json_object_object_add(jobj_data, "power_off_flags", jobj_power_off_flags);


	/* configuration */

	/* serial number (prefix combined with number) */
	char s[32];
	sprintf(s,"%C%d",rxBuffer[PZP_I2C_REG_CONFIG_SERIAL_PREFIX],rxBuffer[PZP_I2C_REG_CONFIG_SERIAL_NUMBER]);
	json_object_object_add(jobj_configuration, "serial_number", json_object_new_string(s));

	json_object_object_add(jobj_configuration, "hardware_model",   json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_HARDWARE_MODEL] ) );
	json_object_object_add(jobj_configuration, "hardware_version", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_HARDWARE_VERSION] ) );

	json_object_object_add(jobj_configuration, "software_model",   json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_SOFTWARE_MODEL] ) );
	json_object_object_add(jobj_configuration, "software_version", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_SOFTWARE_VERSION] ) );

	/* combine software year+2000 with month with day */
	sprintf(s,"20%02d-%02d-%02d",
		rxBuffer[PZP_I2C_REG_CONFIG_SOFTWARE_YEAR],
		rxBuffer[PZP_I2C_REG_CONFIG_SOFTWARE_MONTH],
		rxBuffer[PZP_I2C_REG_CONFIG_SOFTWARE_DAY]
	);
	json_object_object_add(jobj_configuration, "software_date", json_object_new_string(s));

	json_object_object_add(jobj_configuration, "factory_unlocked", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_PARAM_WRITE] ) );

	json_object_object_add(jobj_configuration, "adc_sample_ticks",       json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_TICKS_ADC] ) );
	json_object_object_add(jobj_configuration, "startup_power_on_delay_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_STARTUP_POWER_ON_DELAY] ) );


	json_object_object_add(jobj_configuration, "startup_power_on_delay_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_STARTUP_POWER_ON_DELAY] ) );

	json_object_object_add(jobj_configuration, "command_off_hold_time_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_COMMAND_OFF_HOLD_TIME] ) );
	json_object_object_add(jobj_configuration, "read_watchdog_off_threshold_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_READ_WATCHDOG_OFF_THRESHOLD] ) );
	json_object_object_add(jobj_configuration, "read_watchdog_off_hold_time_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_READ_WATCHDOG_OFF_HOLD_TIME] ) );
	json_object_object_add(jobj_configuration, "write_watchdog_off_threshold_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_WRITE_WATCHDOG_OFF_THRESHOLD] ) );
	json_object_object_add(jobj_configuration, "write_watchdog_off_hold_time_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_WRITE_WATCHDOG_OFF_HOLD_TIME] ) );

	json_object_object_add(jobj_configuration, "lvd-off-threshold_volts", json_object_new_double( adcToVoltage(rxBuffer[PZP_I2C_REG_CONFIG_LVD_DISCONNECT_VOLTAGE]) ) );
	json_object_object_add(jobj_configuration, "lvd-off-delay_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_LVD_DISCONNECT_DELAY] ) );
	json_object_object_add(jobj_configuration, "lvd-on-threshold_volts", json_object_new_double( adcToVoltage(rxBuffer[PZP_I2C_REG_CONFIG_LVD_RECONNECT_VOLTAGE]) ) );

	json_object_object_add(jobj_configuration, "hvd-off-threshold_volts", json_object_new_double( adcToVoltage(rxBuffer[PZP_I2C_REG_CONFIG_HVD_DISCONNECT_VOLTAGE]) ) );
	json_object_object_add(jobj_configuration, "hvd-off-delay_seconds", json_object_new_int( rxBuffer[PZP_I2C_REG_CONFIG_HVD_DISCONNECT_DELAY] ) );
	json_object_object_add(jobj_configuration, "hvd-on-threshold_volts", json_object_new_double( adcToVoltage(rxBuffer[PZP_I2C_REG_CONFIG_HVD_RECONNECT_VOLTAGE]) ) );


	json_object_object_add(jobj, "data", jobj_data);
	json_object_object_add(jobj, "configuration", jobj_configuration);


}

void read_pzpoweri2c(int i2cHandle) {
	uint16_t rxBuffer[CAPACITY_REGISTERS]; 	/* receive buffer */
	uint8_t txBuffer[1];			/* transmit buffer */
	int opResult = 0;			/* for error checking of operations */
	uint8_t address;
	int i;
	int nRegisters=CAPACITY_REGISTERS;

	/* start reading from address 0 */
	txBuffer[0]=0; 	

	if ( 0 != outputDebug ) { 
		fprintf(stderr,"# read_pzpoweri2c() starting\n");
		fprintf(stderr,"# CAPACITY_REGISTERS=%d\n",CAPACITY_REGISTERS);
		fprintf(stderr,"# sizeof(rxBuffer)=%d\n",sizeof(rxBuffer));
		fprintf(stderr,"# txBuffer[0]=0x%02x (this is the starting address)\n",txBuffer[0]);
		fprintf(stderr,"# write(i2cHandle,txBuffer[0],1) starting\n");
	}

	/* write read address */
	opResult = write(i2cHandle, txBuffer, sizeof(txBuffer));

	if ( 0 != outputDebug ) { 
		fprintf(stderr,"# write opResult=%d\n");
	}


	if (opResult != 1) {
		fprintf(stderr,"# No ACK! Exiting...\n");
		exit(2);
	}

	/* clear rxbuffer */
	memset(rxBuffer, 0, sizeof(rxBuffer));
	/* read registers into rxBuffer */
	opResult = read(i2cHandle, rxBuffer, nRegisters*2);

	if ( 0 != outputDebug ) { 
		fprintf(stderr,"# read opResult=%d (bytes read)\n",opResult);

		for ( i=0 ; i<opResult/2 ; i++ ) {
			uint16_t u;

			u=rxBuffer[i];
			fprintf(stderr,"# reg[%03d] = 0x%04x (%5d)",i,u,u);

			if ( u >= 32 && u <= 126 ) {
				fprintf(stderr," '%c'",u);
			} else { 
				fprintf(stderr," ---");
			}

			u=ntohs(rxBuffer[i]);
			fprintf(stderr,"     >>>>> ntohs() 0x%04x (%5d)",u,u);

			if ( u >= 32 && u <= 126 ) {
				fprintf(stderr," '%c'",u);
			} else { 
				fprintf(stderr," ---");
			}

			fprintf(stderr,"<<<<<\n");
		}
	}


	/* results */
	for ( i=0 ; i<nRegisters ; i++ ) {
		/* pzPowerI2C PIC sends high byte and then low byte */
		rxBuffer[i]=ntohs(rxBuffer[i]);
	}

	/* decode data and put into JSON objects */
	decodeRegisters(rxBuffer);
}


void printUsage(void) {
	fprintf(stderr,"Usage:\n\n");
	fprintf(stderr,"switch           argument       description\n");
	fprintf(stderr,"===========================================================================\n");
	fprintf(stderr,"--i2c-device     device         /dev/ entry for I2C-dev device\n");
	fprintf(stderr,"--i2c-address    chip address   hex address of chip\n");
	fprintf(stderr,"--mqtt           none           send data to MQTT\n");
	fprintf(stderr,"--mqtt-host      hostname       MQTT broker\n");
	fprintf(stderr,"--mqtt-port      port number    MQTT broker\n");
	fprintf(stderr,"--mqtt-topic     port number    MQTT topic\n");
	fprintf(stderr,"--debug          none           some additional debugging information\n");
	fprintf(stderr,"--help                          this message\n");
}

void flagProccess(int *flag, char *name) {
	if ( *flag ) {
		fprintf(stderr,"# --%s specified twice. Aborting...\n",name);
		exit(1);
	}

	*flag=1;
	fprintf(stderr,"# action for --%s to be taken\n",name);
}

int rangeCheckInt(char *name, int value, int minValue, int maxValue) {
	if ( value < minValue || value > maxValue ) {
		fprintf(stderr,"# --%s value of %d is outside of range of %d to %d. Aborting...\n",name,value,minValue,maxValue);
		exit(1);
	}

	fprintf(stderr,"# --%s value set to %d\n",name,value);
	return value;
}

double rangeCheckDouble(char *name, double value, double minValue, double maxValue) {
	if ( value < minValue || value > maxValue ) {
		fprintf(stderr,"# --%s value of %f is outside of range of %f to %f. Aborting...\n",name,value,minValue,maxValue);
		exit(1);
	}

	fprintf(stderr,"# --%s value set to %f\n",name,value);
	return value;
}

/* convert voltage (ie 13.8) to ADC reading for this pzPower */
int voltageToADC(double voltage) {
	/* TODO: use calibration value from pzPowerI2C */
	double step = 40.0 / 1024.0;
	
	fprintf(stderr,"# voltage=%f step=%f\n",voltage,step);	

	step = voltage / step;

	fprintf(stderr,"# voltage=%f step=%f\n",voltage,step);	
	
	step = round(step);

	fprintf(stderr,"# voltage=%f step=%f\n",voltage,step);	

	return (int) step;
}

static struct mosquitto * _mosquitto_startup(void) {
	char clientid[24];
	int rc = 0;


	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 23, "pzPowerI2C_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		fprintf(stderr,"# connecting to MQTT server %s:%d\n",action.mqtt_host,action.mqtt_port);
		rc = mosquitto_connect(mosq, action.mqtt_host, action.mqtt_port, 60);

		/* start mosquitto network handling loop */
		mosquitto_loop_start(mosq);
	}
	return	mosq;
}

static void _mosquitto_shutdown(void) {
	fprintf(stderr,"# shutting down mosquitto MQTT connection\n");

	if ( mosq ) {
		/* disconnect mosquitto so we can be done */
		mosquitto_disconnect(mosq);
		/* stop mosquitto network handling loop */
		mosquitto_loop_stop(mosq,0);

		mosquitto_destroy(mosq);
	}

	mosquitto_lib_cleanup();
}

int m_pub(const char *message ) {
	int rc = 0;
	static int messageID;
	/* instance, message ID pointer, topic, data length, data, qos, retain */
	rc = mosquitto_publish(mosq, &messageID, action.mqtt_topic, strlen(message), message, 0, 0); 

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

	/* program flow */
	int exitValue=0;
	int rc;
	char *s;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */


//	uint8_t txBuffer[CAPACITY_BYTES+1];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */
	uint8_t address;

	/* EEPROM stuff */
	int i,j, nTries;


	fprintf(stderr,"# pzPowerI2C utility\n");




	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x1a;		/* default address of pzPower in pzPowerI2C.h */ 	

	action.mqtt=0;
	strcpy(action.mqtt_host,"localhost");
	action.mqtt_port=1883;
	strcpy(action.mqtt_topic,"pzPowerI2C");

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* flags that we set here */
			/* 300 series commands as defined in pzPowerI2C.md */
			{"read",                             no_argument,       0, 300 },
			{"read-loop",                        required_argument, 0, 305 },
			{"read-switch",                      no_argument,       0, 310 }, 
			{"reset-switch-latch",               no_argument,       0, 320 },
			{"reset-write-watchdog",             no_argument,       0, 330 },

			/* 400 series commands as defined in pzPowerI2C.md */
			{"set-command-off",                  required_argument, 0, 400 },
			{"set-command-off-hold-time",        required_argument, 0, 410 },
			{"disable-read-watchdog",            no_argument,       0, 420 },
			{"set-read-watchdog-off-threshold",  required_argument, 0, 430 },
			{"set-read-watchdog-off-hold-time",  required_argument, 0, 440 },
			{"set-write-watchdog-off-threshold", required_argument, 0, 450 },
			{"set-write-watchdog-off-hold-time", required_argument, 0, 460 },

			/* 500 series commands as defined in pzPowerI2C.md */
			{"disable-lvd",                      no_argument,       0, 500 },
			{"set-lvd-off-threshold",            required_argument, 0, 510 },
			{"set-lvd-off-delay",                required_argument, 0, 520 },
			{"set-lvd-on-threshold",             required_argument, 0, 530 },
			{"disable-hvd",                      no_argument,       0, 540 },
			{"set-hvd-off-threshold",            required_argument, 0, 550 },
			{"set-hvd-off-delay",                required_argument, 0, 560 },
			{"set-hvd-on-threshold",             required_argument, 0, 570 },


			/* 10000 series commands as defined in pzPowerI2C.md */
		        {"param",                            required_argument, 0, 10000 },
		        {"set-serial",                       required_argument, 0, 10010 },
		        {"set-adc-ticks",                    required_argument, 0, 10020 },
		        {"set-startup-power-on-delay",       required_argument, 0, 10030 },

			/* normal program */
			{"mqtt",                             no_argument,       0, 'm' },
			{"mqtt-host",                        required_argument, 0, 'H' },
			{"mqtt-port",                        required_argument, 0, 'P' },
			{"mqtt-topic",                       required_argument, 0, 'T' },
		        {"i2c-device",                       required_argument, 0, 'i' },
		        {"i2c-address",                      required_argument, 0, 'a' },
		        {"help",                             no_argument,       0, 'h' },
		        {"debug",                            no_argument,       0, 'd' },
		        {0,                                  0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			/* 300 series */
			case 300: flagProccess(&action.read,"read"); break;
			case 305:
				flagProccess(&action.readLoop,"read-loop"); 
				action.readLoop_value = rangeCheckInt("read-loop",atoi(optarg),0,65534);
				break;
			case 310: flagProccess(&action.readSwitch,"read-switch"); break;
			case 320: flagProccess(&action.resetSwitchLatch,"reset-switch-latch"); break;
			case 330: flagProccess(&action.resetWriteWatchdog,"reset-write-watchdog"); break;


			/* 400 series */
			case 400:
				flagProccess(&action.setCommandOff,"set-command-off"); 
				action.setCommandOff_value = rangeCheckInt("set-command-off",atoi(optarg),0,65534);
				break;
			case 410:
				flagProccess(&action.setCommandOffHoldTime,"set-command-off-hold-time"); 
				action.setCommandOffHoldTime_value = rangeCheckInt("set-command-off-hold-time",atoi(optarg),1,65534);
				break;
			case 420: flagProccess(&action.disableReadWatchdog,"disable-read-watchdog"); break;
			case 430:
				flagProccess(&action.setReadWatchdogOffThreshold,"set-read-watchdog-off-threshold"); 
				action.setReadWatchdogOffThreshold_value = rangeCheckInt("set-read-watchdog-off-threshold",atoi(optarg),1,65534);
				break;
			case 440:
				flagProccess(&action.setReadWatchdogOffHoldTime,"set-read-watchdog-off-hold-time"); 
				action.setReadWatchdogOffHoldTime_value = rangeCheckInt("set-read-watchdog-off-hold-time",atoi(optarg),1,65534);
				break;
			case 450:
				flagProccess(&action.setWriteWatchdogOffThreshold,"set-write-watchdog-off-threshold"); 
				action.setWriteWatchdogOffThreshold_value = rangeCheckInt("set-write-watchdog-off-threshold",atoi(optarg),1,65534);
				break;
			case 460:
				flagProccess(&action.setWriteWatchdogOffHoldTime,"set-write-watchdog-off-hold-time"); 
				action.setWriteWatchdogOffHoldTime_value = rangeCheckInt("set-write-watchdog-off-hold-time",atoi(optarg),1,65534);
				break;

			/* 500 series */
			case 500: flagProccess(&action.disableLVD,"disable-LVD"); break;
			case 510:
				flagProccess(&action.setLVDOffThreshold,"set-lvd-off-threshold"); 
				action.setLVDOffThreshold_value = rangeCheckDouble("set-lvd-off-threshold",atof(optarg),8.0,40.0);
				break;
			case 520:
				flagProccess(&action.setLVDOffDelay,"set-lvd-off-delay"); 
				action.setLVDOffDelay_value = rangeCheckInt("set-lvd-off-delay",atoi(optarg),1,65534);
				break;
			case 530:
				flagProccess(&action.setLVDOnThreshold,"set-lvd-on-threshold"); 
				action.setLVDOnThreshold_value = rangeCheckDouble("set-lvd-on-threshold",atof(optarg),8.0,40.0);
				break;
			case 540: flagProccess(&action.disableHVD,"disable-HVD"); break;
			case 550:
				flagProccess(&action.setHVDOffThreshold,"set-hvd-off-threshold"); 
				action.setHVDOffThreshold_value = rangeCheckDouble("set-hvd-off-threshold",atof(optarg),8.0,40.0);
				break;
			case 560:
				flagProccess(&action.setHVDOffDelay,"set-hvd-off-delay"); 
				action.setHVDOffDelay_value = rangeCheckInt("set-hvd-off-delay",atoi(optarg),1,65534);
				break;
			case 570:
				flagProccess(&action.setHVDOnThreshold,"set-hvd-on-threshold"); 
				action.setHVDOnThreshold_value = rangeCheckDouble("set-hvd-on-threshold",atof(optarg),8.0,40.0);
				break;


			/* 1000 series */
			case 10000:
				flagProccess(&action.param,"param"); 

				if ( 0==strcmp("save",optarg) ) {
					action.param_value=1;
				} else if ( 0==strcmp("defaults",optarg) ) {
					action.param_value=2;
				} else if ( 0==strcmp("reset_cpu",optarg) ) {
					action.param_value=65535;
				} else {
					action.param_value = rangeCheckInt("param",atoi(optarg),0,65535);
				}

				break;
			case 10010:
				flagProccess(&action.setSerial,"set-serial"); 

				i = sscanf(optarg,"%c%d",&action.setSerial_prefix,&j);
				action.setSerial_number=(uint16_t) j;


				if ( action.setSerial_prefix < 'A' || action.setSerial_prefix > 'Z' || j<0 || j>65535 || 2 != i ) {
					fprintf(stderr,"# --set-serial invalid serial number '%s'. Aborting...\n",optarg);
					exit(1);
				}

				action.setSerial=1;

				break;
			case 10020:
				flagProccess(&action.setAdcTicks,"set-adc-ticks"); 
				action.setAdcTicks_value = rangeCheckInt("set-adc-ticks",atoi(optarg),1,65534);
				break;
			case 10030:
				flagProccess(&action.setStartupPowerOnDelay,"set-startup-power-on-delay"); 
				action.setStartupPowerOnDelay_value = rangeCheckInt("set-startup-power-on-delay",atoi(optarg),1,65535);
				break;

			/* getopt / standard program */
			case '?':
				/* getopt error of missing argument or unknown option */
				exit(1);
	
			case 'm':
				action.mqtt=1;
				break;
			case 'd':
				outputDebug=1;
				break;
			case 'H':
				strncpy(action.mqtt_host,optarg,sizeof(action.mqtt_host)-1);
				action.mqtt_host[sizeof(action.mqtt_host)-1]='\0';
				break;
			case 'P':
				sscanf(optarg,"%d",&action.mqtt_port);
				break;
			case 'T':
				strncpy(action.mqtt_topic,optarg,sizeof(action.mqtt_topic)-1);
				action.mqtt_topic[sizeof(action.mqtt_topic)-1]='\0';
				break;

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
		}
	}

	/* start-up verbosity */
	fprintf(stderr,"# using I2C device %s\n",i2cDevice);
	fprintf(stderr,"# using I2C device address of 0x%02X\n",i2cAddress);
	if ( action.mqtt ) {
		fprintf(stderr,"# MQTT enabled to host='%s' port='%d' topic='%s'\n",action.mqtt_host,action.mqtt_port,action.mqtt_topic);
	} else {
		fprintf(stderr,"# MQTT disabled\n");
	}


	/* Open I2C bus */
	int i2cHandle = open(i2cDevice, O_RDWR);

	if ( -1 == i2cHandle ) {
		fprintf(stderr,"# Error opening I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	/* not using 10 bit addresses */
	opResult = ioctl(i2cHandle, I2C_TENBIT, 0);
	/* address of device we will be working with */
	opResult = ioctl(i2cHandle, I2C_SLAVE, i2cAddress);

	/* do initial read and decode of pzPower. We may read and decode again at the end */
	read_pzpoweri2c(i2cHandle);



	/* 300's */
	if ( action.readSwitch ) {
		/* set exit value based on magnetic switch and magnetic latch status */
		int magnetic_switch_state=-1;
		int magnetic_switch_latch=-2;

		json_object *tmp;

		/* read data from JSON object */
		if ( json_object_object_get_ex(jobj_data, "magnetic_switch_state", &tmp) ) {
			magnetic_switch_state=json_object_get_boolean(tmp);
		}
		if ( json_object_object_get_ex(jobj_data, "magnetic_switch_latch", &tmp) ) {
			magnetic_switch_latch=json_object_get_boolean(tmp);
		}


		if ( 0==magnetic_switch_state && 0==magnetic_switch_latch ) {
			exitValue=128;
		} else if ( 0==magnetic_switch_state && 1==magnetic_switch_latch ) {
			exitValue=129;
		} else if ( 1==magnetic_switch_state && 1==magnetic_switch_latch ) {
			exitValue=130;
		} else {
			exitValue=127; /* should not happen */
		}

		fprintf(stderr,"# actionReadSwitch set exitValue=%d\n",exitValue);
	}


	if ( action.resetSwitchLatch ) {
		fprintf(stderr,"# actionResetSwitchLatch clearing magnetic switch latch\n");

		/* writing anything to register PZP_I2C_REG_SWITCH_MAGNET_LATCH clears the magnetic swith latch */
		write_word(i2cHandle,PZP_I2C_REG_SWITCH_MAGNET_LATCH,0);

		action.reRead=1;
	}

	if ( action.resetWriteWatchdog ) {
		fprintf(stderr,"# actionResetWriteWatchdog\n");

		/* writing anything to register PZP_I2C_REG_TIME_WATCHDOG_WRITE_SECONDS restarts the watchdog */
		write_word(i2cHandle,PZP_I2C_REG_TIME_WATCHDOG_WRITE_SECONDS,0);

		action.reRead=1;
	}


	/* 400's */
	if ( action.setCommandOff ) {
		write_word(i2cHandle,PZP_I2C_REG_COMMAND_OFF,action.setCommandOff_value);

		action.reRead=1;
	}

	if ( action.setCommandOffHoldTime ) {
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_COMMAND_OFF_HOLD_TIME,action.setCommandOffHoldTime_value);

		action.reRead=1;
	}

	if ( action.disableReadWatchdog || action.setReadWatchdogOffThreshold ) {
		/* disable read watchdog is the same as setting the threshold to 65535 */
		if ( action.disableReadWatchdog )
			action.setReadWatchdogOffThreshold_value=65535;

		write_word(i2cHandle,PZP_I2C_REG_CONFIG_READ_WATCHDOG_OFF_THRESHOLD,action.setReadWatchdogOffThreshold_value);

		action.reRead=1;
	}

	if ( action.setReadWatchdogOffHoldTime ) {
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_READ_WATCHDOG_OFF_HOLD_TIME,action.setReadWatchdogOffHoldTime_value);

		action.reRead=1;
	}

	if ( action.disableWriteWatchdog || action.setWriteWatchdogOffThreshold ) {
		/* disable write watchdog is the same as setting the threshold to 65535 */
		if ( action.disableWriteWatchdog )
			action.setWriteWatchdogOffThreshold_value=65535;

		write_word(i2cHandle,PZP_I2C_REG_CONFIG_WRITE_WATCHDOG_OFF_THRESHOLD,action.setWriteWatchdogOffThreshold_value);

		action.reRead=1;
	}

	if ( action.setWriteWatchdogOffHoldTime ) {
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_WRITE_WATCHDOG_OFF_HOLD_TIME,action.setWriteWatchdogOffHoldTime_value);

		action.reRead=1;
	}

	if ( action.disableLVD || action.setLVDOffDelay ) {
		/* disable LVD is the same as setting the off delay to 65535 */
		if ( action.disableLVD )
			action.setLVDOffDelay_value=65535;

		write_word(i2cHandle,PZP_I2C_REG_CONFIG_LVD_DISCONNECT_DELAY,action.setLVDOffDelay_value);

		action.reRead=1;
	}

	if ( action.setLVDOffThreshold ) {
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_LVD_DISCONNECT_VOLTAGE,voltageToAdc(action.setLVDOffThreshold_value));

		action.reRead=1;
	}

	if ( action.setLVDOnThreshold ) {
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_LVD_RECONNECT_VOLTAGE,voltageToAdc(action.setLVDOnThreshold_value));

		action.reRead=1;
	}

	if ( action.disableHVD || action.setHVDOffDelay ) {
		/* disable HVD is the same as setting the off delay to 65535 */
		if ( action.disableHVD )
			action.setHVDOffDelay_value=65535;

		write_word(i2cHandle,PZP_I2C_REG_CONFIG_HVD_DISCONNECT_DELAY,action.setHVDOffDelay_value);

		action.reRead=1;
	}

	if ( action.setHVDOffThreshold ) {
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_HVD_DISCONNECT_VOLTAGE,voltageToAdc(action.setHVDOffThreshold_value));

		action.reRead=1;
	}

	if ( action.setHVDOnThreshold ) {
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_HVD_RECONNECT_VOLTAGE,voltageToAdc(action.setHVDOnThreshold_value));

		action.reRead=1;
	}


	/* 10000's */
	if ( action.param ) {
		fprintf(stderr,"# Writing %d to param write register\n",action.param_value);
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_PARAM_WRITE,action.param_value);

		/* sleep 100 ms to allow parameters to be written to EEPROM */
		usleep(100000);

		action.reRead=1;
	}

	if ( action.setSerial ) {
		fprintf(stderr,"# Setting board serial number serialPrefix='%c' serialNumber='%d'\n",action.setSerial_prefix,action.setSerial_number);
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_SERIAL_PREFIX,action.setSerial_prefix);
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_SERIAL_NUMBER,action.setSerial_number);

		action.reRead=1;
	}

	if ( action.setAdcTicks ) {
		fprintf(stderr,"# Writing %d to ADC ticks register\n",action.setAdcTicks_value);
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_TICKS_ADC,action.setAdcTicks_value);

		action.reRead=1;
	}

	if ( action.setStartupPowerOnDelay ) {
		fprintf(stderr,"# Writing %d to startup power on delay register\n",action.setStartupPowerOnDelay_value);
		write_word(i2cHandle,PZP_I2C_REG_CONFIG_STARTUP_POWER_ON_DELAY,action.setStartupPowerOnDelay_value);
		action.reRead=1;
	}

	/* attempt to start mosquitto */
	if ( action.mqtt && 0 == _mosquitto_startup() ) {
		return	1;
	}

	
	do {
		if ( action.reRead ) {
			/* release JSON objects */
			json_object_put(jobj_enclosing);
			json_object_put(jobj);
			json_object_put(jobj_configuration); 
			json_object_put(jobj_power_off_flags); 
			json_object_put(jobj_data); 

			/* re read and create new JSON objects */
			read_pzpoweri2c(i2cHandle);
		}

		/* print JSON output */
		/* enclose array */
		jobj_enclosing = json_object_new_object();
		json_object_object_add(jobj_enclosing, "pzPowerI2C", jobj);

		/* convert JSON object to string */
		s = (char *) json_object_to_json_string_ext(jobj_enclosing, JSON_C_TO_STRING_PRETTY);

		/* print to stdout */
		printf("%s\n", s);

		/* send to MQTT */
		if ( action.mqtt ) {
			rc =  m_pub(s);
		}
		

		if ( action.readLoop ) {
			action.reRead=1;
			/* wait interval */
			fprintf(stderr,"# sleeping %d seconds before next read\n",action.readLoop_value);
			sleep(action.readLoop_value);
		} else {
			action.reRead=0;
		}
	} while ( action.reRead );

	/* release JSON objects */
	json_object_put(jobj);
	json_object_put(jobj_configuration); 
	json_object_put(jobj_power_off_flags); 
	json_object_put(jobj_data); 

	/* shut down MQTT */
	if ( action.mqtt ) {
		_mosquitto_shutdown();
	}


	/* close I2C */
	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");

	exit(exitValue);
}
