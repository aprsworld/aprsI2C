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
	int startAddress;
	int nBytes;
	char *inFilename, *outFilename;

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
	startAddress=0;
	nBytes=8192;
	inFilename=outFilename=NULL;

	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x50; 		/* 0b1010(A2)(A1)(A0) */

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
		        {"read",           required_argument, 0, 'r' },
		        {"write",          required_argument, 0, 'w' },
		        {"dump",           no_argument,       0, 'd' },
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

	txBuffer[0]=0x00;
	txBuffer[1]=0x00;
	sprintf(txBuffer+2,"short message");
	opResult = write(i2cHandle, txBuffer, strlen(txBuffer+2)+2);
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

		fprintf(stderr,"# Dump from EEPROM\n");
		for ( i=0 ; i<sizeof(rxBuffer) ; i++ ) {
			putchar(rxBuffer[i]);
		}
	} else if ( NULL != outFilename ) {
		fprintf(stderr,"# outfile file: %s\n",outFilename);

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
		int written = fwrite(rxBuffer,1,nBytes,fp);
		if ( written != nBytes ) {
			fprintf(stderr,"# Error writing EEPROM data to output file. %d bytes written.\n# Exiting...\n",written);
			exit(1);

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

