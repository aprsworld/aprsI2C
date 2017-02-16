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

/* number of acknowledgement cycles to poll on write for */
#define TIMEOUT_NTRIES 50


int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* program flow */
	int dumpRead; 
	int startAddress;
	int nBytes;
	char *inFilename, *outFilename;
	int breakOnNull;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */


	char rxBuffer[8192];	/* receive buffer */
	char txBuffer[33+1];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */

	/* EEPROM stuff */
	int i, nTries;


	fprintf(stderr,"# ee_2464 24AA64 / 24LC64 EEPROM I2C utility\n");

	/* defaults and command line arguments */
	dumpRead=0;
	startAddress=0;
	nBytes=8192;
	inFilename=outFilename=NULL;
	breakOnNull=0;

	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x50; 		/* 0b1010(A2)(A1)(A0) */

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
		        {"read",           required_argument, 0, 'r' },
		        {"write",          required_argument, 0, 'w' },
		        {"dump",           no_argument,       0, 'd' },
		        {"string",         no_argument,       0, 'b' },
		        {"start-address",  required_argument, 0, 's' },
		        {"n-bytes",        required_argument, 0, 'n' },
		        {"i2c-device",     required_argument, 0, 'i' },
		        {"i2c-address",    required_argument, 0, 'a' },
		        {0,                0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'b':
				breakOnNull=1;
				break;
			case 's':
				startAddress=atoi(optarg);
				if ( startAddress<0 || startAddress>8191 ) {
					fprintf(stderr,"# start address out of range (0 to 8191)\n# Exiting...\n");
					exit(1);
				} 
				break;
			case 'n':
				nBytes=atoi(optarg);
				if ( nBytes<1 || nBytes>8192 ) {
					fprintf(stderr,"# number of bytes out of range (1 to 8192)\n# Exiting...\n");
					exit(1);
				} 
				break;
			case 'w':
				inFilename=optarg;
				break;
			case 'r':
				outFilename=optarg;
				break;
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

	if ( NULL != inFilename ) {
		fprintf(stderr,"# input file: %s\n",inFilename);
	}

	fprintf(stderr,"# start address: %d\n",startAddress);
	fprintf(stderr,"# bytes to read / write: %d\n",nBytes);


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


	/* write operation if needed */
	if ( NULL != inFilename ) {
		fprintf(stderr,"# input file: %s\n",inFilename);
		if ( breakOnNull ) {
			fprintf(stderr,"# write string mode (stopping at first null)\n");
		} else {
		 	fprintf(stderr,"# write binary mode\n");
		}

		/* open input file */
		FILE *fp=fopen(inFilename,"r");
		if ( NULL == fp ) {
			fprintf(stderr,"# Error opening input file in read mode.\n# %s\n# Exiting...\n",strerror(errno));
			exit(1);
		}

		while (1) { 
			/* EEPROM write is in 32 byte or less chunks. After writing, we must poll for ack to know that write is done */

			/* read up to 32 bytes */
			for ( i=2 ; i<32+2 ; i++ ) {
				txBuffer[i] = fgetc(fp);

				if ( EOF == txBuffer[i] ) {
					break;
				}

				if ( breakOnNull && '\0' == txBuffer[i] ) {
					break;
				}
			}
			fprintf(stderr,"# i=%d\n",i);

	

			/* address high byte */
			txBuffer[0] = ((startAddress>>8) & 0b00011111);
			/* address low byte */
			txBuffer[1] = (startAddress & 0b11111111);

			int j;
			for ( j=0 ; j<32+2 ; j++ ) {
				fprintf(stderr,"# txBuffer[%d] = 0x%02X\n",j,txBuffer[j]);

			}

			/* fixme */
			break;
		}

		/* close output file */
		if ( 0 != fclose(fp) ) {
			fprintf(stderr,"# Error closing input file.\n# %s\n# Exiting...\n",strerror(errno));
			exit(1);
		}
	}

#if 0
	txBuffer[0]=0x00;
	txBuffer[1]=0x00;
	sprintf(txBuffer+2,"short message...");
	opResult = write(i2cHandle, txBuffer, strlen(txBuffer+2)+3); /* +3 is for two address bytes + 1 null */
	fprintf(stderr,"opResult=%d\n",opResult);

	/* poll for acknowledgement so we can write the next page */
	for ( nTries=0 ; ; nTries++ ) {
		opResult = write(i2cHandle,txBuffer,0);
		fprintf(stderr,"poll nTries=%d opResult=%d\n",nTries,opResult);

		if ( opResult != -1 ) 
			break;

		if ( nTries >= TIMEOUT_NTRIES ) {
			fprintf(stderr,"# Timeout while polling for write acknowledgement! Exiting...\n");
			exit(2);
		}
	}
#endif


	/* do any requested reading after sets */
	if ( dumpRead ) {

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

		fprintf(stderr,"# Dump from EEPROM\n");
		for ( i=0 ; i<sizeof(rxBuffer) ; i++ ) {
			putchar(rxBuffer[i]);
		}
	} else if ( NULL != outFilename ) {
		fprintf(stderr,"# output file: %s\n",outFilename);
		if ( breakOnNull ) {
			fprintf(stderr,"# read string mode (stopping before first null)\n");
		} else {
		 	fprintf(stderr,"# read binary mode\n");
		}

		/* open output file */
		FILE *fp=fopen(outFilename,"w");
		if ( NULL == fp ) {
			fprintf(stderr,"# Error opening output file in write mode.\n# %s\n# Exiting...\n",strerror(errno));
			exit(1);
		}

		/* address high byte */
		txBuffer[0] = ((startAddress>>8) & 0b00011111);
		/* address low byte */
		txBuffer[1] = (startAddress & 0b11111111);

		/* write read address */
		opResult = write(i2cHandle, txBuffer, 2);
		if (opResult != 2) {
			fprintf(stderr,"# No ACK! Exiting...\n");
			exit(2);
		}

		/*  read buffer length */
		memset(rxBuffer, 0, sizeof(rxBuffer));
		opResult = read(i2cHandle, rxBuffer, nBytes);

		/* write to file */
		if ( breakOnNull ) {
			for ( i=0 ; i<nBytes && '\0' != rxBuffer[i] ; i++ ) {
				if ( EOF == fputc(rxBuffer[i],fp) ) {
					fprintf(stderr,"# Error writing EEPROM data to output file. %d bytes written.\n# Exiting...\n",i-1);
					exit(1);
				}
			}
		} else {
			int written = fwrite(rxBuffer,1,nBytes,fp);
			if ( written != nBytes ) {
				fprintf(stderr,"# Error writing EEPROM data to output file. %d bytes written.\n# Exiting...\n",written);
				exit(1);
			}
		}
		

		/* close output file */
		if ( 0 != fclose(fp) ) {
			fprintf(stderr,"# Error closing output file.\n# %s\n# Exiting...\n",strerror(errno));
			exit(1);
		}
	}


	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");
	
	exit(0);
}

