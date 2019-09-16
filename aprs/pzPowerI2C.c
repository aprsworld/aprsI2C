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
#include <json.h>
 
extern char *optarg;
extern int optind, opterr, optopt;

/* processed data read from pzPowerI2C at start */
struct json_object *jobj, *jobj_data,*jobj_configuration;

/* number of acknowledgement cycles to poll on write for */
#define TIMEOUT_NTRIES 50

/* byte capacity of device */
#define CAPACITY_BYTES 128

/* actions to take */
typedef struct {
	int read;
	int readSwitch;
	int resetSwitchLatch;

	int setSerial;
	uint8_t setSerial_prefix;
	uint16_t setSerial_number;
	

	int setCommandOff;
	double setCommandOff_value;

	int setCommandOffHoldTime;
	int setCommandOffHoldTime_value;

	int disableReadWatchdog;

	int setReadWatchdogOffThreshold;
	int setReadWatchdogOffThreshold_value;
	
} struct_action;

/* global structures */
struct_action action={0};

int write_word(int i2cHandle, uint8_t address, uint16_t value) {
	uint8_t txBuffer[3];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */

	/* build tx buffer to send */
	/* address */
	txBuffer[0] = address;
	/* value */
	txBuffer[1]=(value >> 8) & 0xff;
	txBuffer[2]=value & 0xff;

	/* write */
	opResult = write(i2cHandle, txBuffer, sizeof(txBuffer));
	if ( -1 == opResult ) {
		fprintf(stderr,"# Error writing to address. %s\n",strerror(errno));

		return -1;
	}

	return 0;
}


double ntcThermistor(double voltage, double beta, double beta25, double rSource, double vSource) {
	double rt, tKelvin;

	rt = (voltage*rSource)/(vSource-voltage);

	tKelvin=1.0/( (1.0/beta) * log(rt/beta25) + 1.0/298.15);

	return tKelvin-273.15;
}

void decodeRegisters(uint16_t *rxBuffer) {
	int i;
 
	/*
	 * The following create an object and add the question and answer to it.
	 */
	jobj = json_object_new_object();
	jobj_data = json_object_new_object();
	jobj_configuration = json_object_new_object();


	/* data */

	/* input voltage */
	json_object_object_add(jobj_data, "voltage_in_now", json_object_new_double((40.0/1024.0)*rxBuffer[0]));
	json_object_object_add(jobj_data, "voltage_in_average", json_object_new_double((40.0/1024.0)*rxBuffer[1]));

	/* temperature of board from onboard thermistor */
	double t;
	t=ntcThermistor( rxBuffer[2], 3977, 10000, 10000, 1024);
	json_object_object_add(jobj_data, "temperature_pcb_now", json_object_new_double(t));

	t=ntcThermistor( rxBuffer[3], 3977, 10000, 10000, 1024);
	json_object_object_add(jobj_data, "temperature_pcb_average", json_object_new_double(t));

	/* magnetic switch */
	json_object_object_add(jobj_data,"magnetic_switch_state", json_object_new_boolean(rxBuffer[4]));
	json_object_object_add(jobj_data,"magnetic_switch_latch", json_object_new_boolean(rxBuffer[5]));

	/* status */
	json_object_object_add(jobj_data,"sequence_number",       json_object_new_int( rxBuffer[6] ) );
	json_object_object_add(jobj_data,"interval_milliseconds", json_object_new_int( rxBuffer[7] ) );
	json_object_object_add(jobj_data,"uptime_minutes",        json_object_new_int( rxBuffer[8] ) );
	json_object_object_add(jobj_data,"read_watchdog_seconds",      json_object_new_int( rxBuffer[9] ) );
	json_object_object_add(jobj_data,"write_watchdog_seconds",      json_object_new_int( rxBuffer[10] ) );


	/* configuration */

	/* serial number (prefix combined with number) */
	char s[16];
	sprintf(s,"%C%d",rxBuffer[32],rxBuffer[33]);
	json_object_object_add(jobj_configuration, "serial_number",            json_object_new_string(s));

	json_object_object_add(jobj_configuration, "adc_sample_ticks",         json_object_new_int( rxBuffer[38] ) );
	json_object_object_add(jobj_configuration, "watchdog_seconds_max",     json_object_new_int( rxBuffer[39] ) );

	json_object_object_add(jobj_configuration, "power_startup_state_host", json_object_new_int( rxBuffer[41] ) );




	json_object_object_add(jobj, "data", jobj_data);
	json_object_object_add(jobj, "configuration", jobj_configuration);

 
	printf("%s\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY));

}

void printUsage(void) {
	fprintf(stderr,"Usage:\n\n");
	fprintf(stderr,"switch           argument       description\n");
	fprintf(stderr,"========================================================================================================\n");
	fprintf(stderr,"--i2c-device     device         /dev/ entry for I2C-dev device\n");
	fprintf(stderr,"--i2c-address    chip address   hex address of chip\n");
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

int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* program flow */

	int exitValue=0;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */


	uint16_t rxBuffer[CAPACITY_BYTES];	/* receive buffer */
	uint8_t txBuffer[CAPACITY_BYTES+1];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */
	uint8_t address;

	/* EEPROM stuff */
	int i,j, nTries;


	fprintf(stderr,"# pzPowerI2C utility\n");




	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x1a;		/* default address of pzPower in pzPowerI2C.h */ 	

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* flags that we set here */
			{"read",                      no_argument,       0, 260 },
			{"read-switch",               no_argument,       0, 270 }, 
			{"reset-switch-latch",        no_argument,       0, 280 },
		        {"set-serial",                required_argument, 0, 290 },
			{"set-command-off",           required_argument, 0, 300 },
			{"set-command-off-hold-time", required_argument, 0, 310},
			{"disable-read-watchdog",     no_argument,       0, 320 },

		        {"i2c-device",                required_argument, 0, 'i' },
		        {"i2c-address",               required_argument, 0, 'a' },
		        {"help",                      no_argument,       0, 'h' },
		        {0,                           0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			/* simple flag sets */
			case 260: flagProccess(&action.read,"read"); break;
			case 270: flagProccess(&action.readSwitch,"read-switch"); break;
			case 280: flagProccess(&action.resetSwitchLatch,"reset-switch-latch"); break;
			case 320: flagProccess(&action.disableReadWatchdog,"disable-read-watchdog"); break;

			/* options with arguments */
			case 290:
				flagProccess(&action.setSerial,"set-serial"); 

				i = sscanf(optarg,"%c%d",&action.setSerial_prefix,&j);
				action.setSerial_number=(uint16_t) j;


				if ( action.setSerial_prefix < 'A' || action.setSerial_prefix > 'Z' || j<0 || j>65535 || 2 != i ) {
					fprintf(stderr,"# --set-serial invalid serial number '%s'. Aborting...\n",optarg);
					exit(1);
				}

				action.setSerial=1;

				break;
			case 300:
				flagProccess(&action.setCommandOff,"set-command-off"); 
				action.setCommandOff_value = rangeCheckInt("set-command-off",atoi(optarg),0,65534);
				break;
			case 310:
				flagProccess(&action.setCommandOffHoldTime,"set-command-off-hold-time"); 
				action.setCommandOffHoldTime_value = rangeCheckInt("set-command-off-hold-time",atoi(optarg),1,65534);
				break;

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
		}
	}

	/* start-up verbosity */
	fprintf(stderr,"# using I2C device %s\n",i2cDevice);
	fprintf(stderr,"# using I2C device address of 0x%02X\n",i2cAddress);


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

	/* read pzPowerI2C registers in all cases */
	if ( 1 ) {
		int startAddress=0;
		int nRegisters=42;

		txBuffer[0] = ( (startAddress*2) & 0b11111111);

//		fprintf(stderr,"# txBuffer[0]=0x%02x\n",txBuffer[0]);

		/* write read address */
		opResult = write(i2cHandle, txBuffer, 1);


		if (opResult != 1) {
			fprintf(stderr,"# No ACK! Exiting...\n");
			exit(2);
		}

		/* read registers into rxBuffer */
		memset(rxBuffer, 0, sizeof(rxBuffer));
		opResult = read(i2cHandle, rxBuffer, nRegisters*2);
//		fprintf(stderr,"# %d bytes read starting at register %d\n",opResult,startAddress);

		/* results */
		for ( i=0 ; i<nRegisters ; i++ ) {
			/* pzPowerI2C PIC sends high byte and then low byte */
			rxBuffer[i]=ntohs(rxBuffer[i]);

#if 0
			fprintf(stderr,"# reg[%03d] = 0x%04x (%5d)",i,rxBuffer[i],rxBuffer[i]);

			if ( rxBuffer[i] >= 32 && rxBuffer[i] <= 126 ) {
				fprintf(stderr," '%c'",rxBuffer[i]);
			} 
			fprintf(stderr,"\n");
#endif

		}

		/* decode data and put into JSON objects */
		decodeRegisters(rxBuffer);
	}

	/* read complete. Now go on to to any additional actions */



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

		/* writing anything to register 5 clears the magnetic swith latch */
		opResult = write_word(i2cHandle,5,0);
		if ( 0 != opResult ) {
			fprintf(stderr,"# write_word returned %d\n,",opResult);
			exit(1);
		}
	}

	if ( action.setSerial ) {
		fprintf(stderr,"# Setting board serial number serialPrefix='%c' serialNumber='%d'\n",action.setSerial_prefix,action.setSerial_number);
		write_word(i2cHandle,32,action.setSerial_prefix);
		write_word(i2cHandle,33,action.setSerial_number);
	}

	if ( action.setCommandOff ) {
		/* TODO simple write */
	}

	if ( action.setCommandOffHoldTime ) {
		/* TODO simple write */

	}

	if ( action.disableReadWatchdog || action.setReadWatchdogOffThreshold ) {
		/* disable read watchdog is the same as setting the threshold to 65535 */
		if ( action.disableReadWatchdog )
			action.setReadWatchdogOffThreshold_value=65535;

		/* TODO simple write */
	
	}



	/* program clean-up and shut down */
	/* delete the JSON objects */
	json_object_put(jobj);
	json_object_put(jobj_configuration); 
	json_object_put(jobj_data); 


	/* close I2C */
	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");


	
	exit(exitValue);
}

