#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>

extern char *optarg;
extern int optind, opterr, optopt;



int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* program flow */
	char ramSetBuffer[57];
	char dateSetBuffer[20]; 
	int dumpRead; 

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */


	char rxBuffer[8192];	/* receive buffer */
	char txBuffer[33+1];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */

	/* EEPROM stuff */
	int i;


	fprintf(stderr,"# ee_2464 24AA64 / 24LC64 EEPROM I2C utility\n");

	/* defaults and command line arguments */
	memset(ramSetBuffer, 0, sizeof(ramSetBuffer));
	memset(dateSetBuffer, 0, sizeof(dateSetBuffer));
	dumpRead=0;
	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x50; 		/* 0b1010(A2)(A1)(A0) */

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
		        {"dump",        no_argument,       0, 'd' },
		        {"i2c-device",  required_argument, 0, 'i' },
		        {"i2c-address", required_argument, 0, 'a' },
		        {0,          0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'a':
				sscanf(optarg,"%x",&i2cAddress);
				break;
			case 'i':
				strncpy(i2cDevice,optarg,sizeof(i2cDevice)-1);
				i2cDevice[sizeof(i2cDevice)-1]='\0';
				break;
			case 'd':
				dumpRead=1;
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

	txBuffer[0]=0x00;
	txBuffer[1]=0x00;
	sprintf(txBuffer+2,"hello!");
	opResult = write(i2cHandle, txBuffer, 8);
	fprintf(stderr,"opResult=%d\n",opResult);
	sleep(1);

#if 0
	/* do sets first */
	if ( strlen(dateSetBuffer) ) {
		fprintf(stderr,"# Attempting to set date with '%s'\n",dateSetBuffer);

		if ( 0 != ds1307_build_date_from_string(dateSetBuffer, txBuffer+1) ) {
			fprintf(stderr,"# invalid date format or value. Exiting...\n");
			exit(3);
		}

		txBuffer[0]=0x00; /* address of clock registers */
		/* txBuffer should now be filled with address of clock register and 8 bytes of clock data. Now we can write. */

		opResult = write(i2cHandle, txBuffer, 9);
		if (opResult != 9) {
			fprintf(stderr,"# Error writing date. write() returned %d instead of 9. Exiting...\n",opResult);
			exit(2);
		}
	}

	if ( strlen(ramSetBuffer) ) {
		fprintf(stderr,"# writing up to 56 bytes of '%s' (padded with \\0 if less than 56 bytes) to RAM\n",ramSetBuffer);

		txBuffer[0]=0x08; /* address of clock registers */
		memcpy(txBuffer+1,ramSetBuffer,56);
		/* txBuffer should now be filled with address of first RAM register and 56 bytes of ram data. Now we can write. */

		opResult = write(i2cHandle, txBuffer, 57);
		if (opResult != 57) {
			fprintf(stderr,"# Error writing RAM data. write() returned %d instead of 57. Exiting...\n",opResult);
			exit(2);
		}
	}
#endif


	/* do any requested reading after sets */
	if ( dumpRead ) {

		/* control byte */
//		txBuffer[0] = 0b10100000; 
		/* address high byte */
		txBuffer[0] = 0x00;
		/* address low byte */
		txBuffer[1] = 0x00;

		/* write read address */
		opResult = write(i2cHandle, txBuffer, 2);
		if (opResult != 2) {
			fprintf(stderr,"# No ACK! Exiting...\n");
			exit(2);
		}

		/*  read buffer length */
		memset(rxBuffer, 0, sizeof(rxBuffer));
		opResult = read(i2cHandle, rxBuffer, sizeof(rxBuffer));

		if ( dumpRead) {
			fprintf(stderr,"# Dump from EEPROM\n");
			for ( i=0 ; i<sizeof(rxBuffer) ; i++ ) {
				putchar(rxBuffer[i]);
			}
		}
	}
	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");
	
	exit(0);
}

