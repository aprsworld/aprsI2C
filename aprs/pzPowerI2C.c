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

extern char *optarg;
extern int optind, opterr, optopt;

/* number of acknowledgement cycles to poll on write for */
#define TIMEOUT_NTRIES 50

/* byte capacity of device */
#define CAPACITY_BYTES 128

int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* program flow */
	int powerHostOnSeconds = -1;
	int powerHostOffSeconds = -1;
	int powerNetOnSeconds = -1;
	int powerNetOffSeconds = -1;
	int actionRead = 0;
	int actionReadSwitch = 0;
	int actionResetSwitchLatch = 0;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */


	char rxBuffer[CAPACITY_BYTES];	/* receive buffer */
	char txBuffer[33+1];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */

	/* EEPROM stuff */
	int i, nTries;


	fprintf(stderr,"# pzPowerI2C utility\n");



	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x22; 	

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
		        {"power-host-on",       required_argument, 0, 'P' },
		        {"power-host-off",      required_argument, 0, 'p' },
		        {"power-net-on",        required_argument, 0, 'N' },
		        {"power-net-off",       required_argument, 0, 'n' },
			{"read",                no_argument,       0, 'r' },
			{"read-switch",         no_argument,       0, 's' },
			{"reset-swtitch-latch", no_argument,       0, 'S' },
		        {"i2c-device",          required_argument, 0, 'i' },
		        {"i2c-address",         required_argument, 0, 'a' },
		        {"help",                no_argument,       0, 'h' },
		        {0,                     0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'r':
				actionRead=1;
				break;
			case 's':
				actionReadSwitch=1;
				break;
			case 'S':
				actionResetSwitchLatch=1;
				break;
			case 'h':
				printf("switch           argument       description\n");
				printf("========================================================================================================\n");
				printf("--read           filename       read pzPowerI2C and send JSON formatted data to stdout\n");
				printf("--power-host-on  seconds        turn on power switch to host after seconds delay.\n");
				printf("--power-host-off seconds        turn off power switch to host after seconds delay.\n");
				printf("--power-net-on   seconds        turn on power switch to external USB device after seconds delay.\n");
				printf("--power-net-off  seconds        turn off power switch to external USB device after seconds delay.\n");
				printf("--start-address  address        starting EEPROM address\n");
				printf("--i2c-device     device         /dev/ entry for I2C-dev device\n");
				printf("--i2c-address    chip address   hex address of chip\n");
				printf("--help                          this message\n");
				exit(0);	
				break;
			case 'P':
				powerHostOnSeconds=atoi(optarg);
				if ( powerHostOnSeconds<0 || powerHostOnSeconds>65534 ) {
					fprintf(stderr,"# delay seconds out of range. (0 (no delay) to 65534 seconds)\n# Exiting...\n");
					exit(1);
				} 
				break;
			case 'p':
				powerHostOffSeconds=atoi(optarg);
				if ( powerHostOffSeconds<0 || powerHostOffSeconds>65534 ) {
					fprintf(stderr,"# delay seconds out of range. (0 (no delay) to 65534 seconds)\n# Exiting...\n");
					exit(1);
				} 
				break;
			case 'N':
				powerNetOnSeconds=atoi(optarg);
				if ( powerNetOnSeconds<0 || powerNetOnSeconds>65534 ) {
					fprintf(stderr,"# delay seconds out of range. (0 (no delay) to 65534 seconds)\n# Exiting...\n");
					exit(1);
				} 
				break;
			case 'n':
				powerNetOffSeconds=atoi(optarg);
				if ( powerNetOffSeconds<0 || powerNetOffSeconds>65534 ) {
					fprintf(stderr,"# delay seconds out of range. (0 (no delay) to 65534 seconds)\n# Exiting...\n");
					exit(1);
				} 
				break;
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

#if 0
	/* write operation if needed */
	if ( NULL != inFilename ) {
		fprintf(stderr,"# input file: %s\n",inFilename);
		if ( stringMode ) {
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

		int done=0;
		int bytesRead=0;
		int truncated=0;
		/* 
		write can start anywhere, but we must write <= page length (32 bytes). So if we want to start at address 30,
		for example, then we can only write address 30 and address 31 in this page 
	
		Examples:
		address	bytesWeCanWrite
		0	32 (0, 1, 2, ..., 31)
		30	2  (30, 31)
		31	1  (31)
		32	32 (0, 1, 2, ..., 31)

		bytesWeCanWrite = 32-(address%32)

		*/
		int address=startAddress;
		/* EEPROM write is in 32 byte or less chunks. After writing, we must poll for ack to know that write is done */
		do {
			int bytesWeCanWrite = 32-(address%32);

//			printf("# start of do {} loop: address=%d bytesWeCanWrite=%d\n",address,bytesWeCanWrite);

			/* read up to bytesWeCanWrite bytes + save two bytes for address */
			for ( i=2 ; i<(bytesWeCanWrite+2) && bytesRead < nBytes ; i++,bytesRead++ ) {
				int c=fgetc(fp);

				txBuffer[i] = c;

				if ( (EOF == c) || ( stringMode && '\0' == txBuffer[i] ) ) {
					done=1;
					break;
				}
			}

			if ( bytesRead >= nBytes ) {
				fprintf(stderr,"# WARNING: truncating input\n");
				truncated=1;
				done=1;
			}


			/* address high byte */
			txBuffer[0] = ((address>>8) & 0b00011111);
			/* address low byte */
			txBuffer[1] = (address & 0b11111111);

			/* write */
			opResult = write(i2cHandle, txBuffer, i);

			/* poll for acknowledgement so we can write the next page */
			for ( nTries=0 ; ; nTries++ ) {
				opResult = write(i2cHandle,txBuffer,0);
//				fprintf(stderr,"poll nTries=%d opResult=%d\n",nTries,opResult);

				if ( opResult != -1 ) 
					break;

				if ( nTries >= TIMEOUT_NTRIES ) {
					fprintf(stderr,"# Timeout while polling for write acknowledgement! Exiting...\n");
					exit(2);
				}
			}

			address += bytesWeCanWrite;
		} while ( ! done );

		/* 
		if we need to null terminate, we now do that as a separate write 
		
		if we have room left before hitting nBytes limit, we put '\0' after data.
		if we are out of room, then we replace the last data byte with a '\0'
		*/
		if ( stringMode ) {
			int nullAddress;


			if ( truncated ) {
				/* replace last byte */
				nullAddress=startAddress+bytesRead-1;
				fprintf(stderr,"# WARNING: replacing last byte with null at address %d due to --string mode.\n",nullAddress);
			} else {
				/* put after last byte */
				nullAddress=startAddress+bytesRead;
				bytesRead++;
				fprintf(stderr,"# adding null after last byte. null at address %d due to --string mode.\n",nullAddress);
			}

			/* address high byte */
			txBuffer[0] = ((nullAddress>>8) & 0b00011111);
			/* address low byte */
			txBuffer[1] = (nullAddress & 0b11111111);
			/* null byte */
			txBuffer[2] = '\0';

			/* write */
			opResult = write(i2cHandle, txBuffer, 3);

			/* poll for acknowledgement so we can write the next page */
			for ( nTries=0 ; ; nTries++ ) {
				opResult = write(i2cHandle,txBuffer,0);
//				fprintf(stderr,"poll nTries=%d opResult=%d\n",nTries,opResult);

				if ( opResult != -1 ) 
					break;

				if ( nTries >= TIMEOUT_NTRIES ) {
					fprintf(stderr,"# Timeout while polling for write acknowledgement! Exiting...\n");
					exit(2);
				}
			}
		} 

		/* close output file */
		if ( 0 != fclose(fp) ) {
			fprintf(stderr,"# Error closing input file.\n# %s\n# Exiting...\n",strerror(errno));
			exit(1);
		}

		fprintf(stderr,"# wrote %d bytes to EEPROM\n",bytesRead);
	}


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
		if ( stringMode ) {
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
		fprintf(stderr,"# %d bytes read\n",opResult);

		/* write to file */
		if ( stringMode ) {
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
#endif


	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");
	
	exit(0);
}

