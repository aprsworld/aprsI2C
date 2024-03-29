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

/* 
byte capacity of EEPROM:
this device is a little different. It is 256 bytes, but the upper 128 bytes are write protected 
and the top most 6 bytes have a globally unique MAC address programmed by microchip. 
So address 0x80 to 0xfa are write product and contain nothing from the factory.
So we'll set the capacity at 128 bytes, and have a --read-mac option as the only thing that accesses that extra space
*/
#define CAPACITY_BYTES 128

int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* program flow */
	int macRead;
	int dumpRead; 
	int startAddress;
	int nBytes;
	char *inFilename, *outFilename;
	int stringMode;

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */


	char rxBuffer[CAPACITY_BYTES];	/* receive buffer */
	char txBuffer[33+1];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */

	/* EEPROM stuff */
	int i, nTries;


	fprintf(stderr,"# mac_24AA02E48T MAC address EEPROM I2C utility\n");

	/* defaults and command line arguments */
	macRead=0;
	dumpRead=0;
	startAddress=0;
	nBytes=CAPACITY_BYTES;
	inFilename=outFilename=NULL;
	stringMode=0;

	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x50; 		/* 0b1010(A2)(A1)(A0) */

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
		        {"read-mac",       no_argument,       0, 'm' },
		        {"read",           required_argument, 0, 'r' },
		        {"write",          required_argument, 0, 'w' },
		        {"dump",           no_argument,       0, 'd' },
		        {"string",         no_argument,       0, 'b' },
		        {"start-address",  required_argument, 0, 's' },
		        {"n-bytes",        required_argument, 0, 'n' },
		        {"i2c-device",     required_argument, 0, 'i' },
		        {"i2c-address",    required_argument, 0, 'a' },
		        {"capacity",       no_argument,       0, 'c' },
		        {"help",           no_argument,       0, 'h' },
		        {0,                0,                 0,  0 }
		};

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'h':
				printf("switch           argument       description\n");
				printf("========================================================================================================\n");
				printf("--read-mac                      read MAC address from EEPROM\n");
				printf("--read           filename       read EEPROM and write to filename\n");
				printf("--write          filename       write contents of filename to EEPROM\n");
				printf("--dump                          dump entire contents of EEPROM to stdout\n");
				printf("--string                        null terminate written content. Or read EEPROM until null encountered\n");
				printf("--start-address  address        starting EEPROM address\n");
				printf("--n-bytes        bytes          read/write n-bytes or up to n-bytes when in --string mode\n");
				printf("--i2c-device     device         /dev/ entry for I2C-dev device\n");
				printf("--i2c-address    chip address   hex address of chip\n");
				printf("--capacity                      print capacity of EEPROM to stdout and exit\n");
				printf("--help                          this message\n");
				exit(0);	
			case 'c':
				printf("%d\n",CAPACITY_BYTES);
				exit(1);
			case 'b':
				stringMode=1;
				break;
			case 's':
				startAddress=atoi(optarg);
				if ( startAddress<0 || startAddress>8191 ) {
					fprintf(stderr,"# start address out of range (0 to %d)\n# Exiting...\n",CAPACITY_BYTES);
					exit(1);
				} 
				break;
			case 'n':
				nBytes=atoi(optarg);
				if ( nBytes<1 || nBytes>CAPACITY_BYTES ) {
					fprintf(stderr,"# number of bytes out of range (1 to %d)\n# Exiting...\n",CAPACITY_BYTES);
					exit(1);
				} 
				if ( nBytes+startAddress>CAPACITY_BYTES ) {
					fprintf(stderr,"# nBytes+startAddress=%d which exceeds CAPACITY_BYTES byte capacitor of EEPROM.\n# Exiting...\n",nBytes+startAddress);
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
			case 'm':
				macRead=1;
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
		write can start anywhere, but we must write <= page length (8 bytes). So if we want to start at address 6,
		for example, then we can only write address 6 and address 7 in this page 
		*/
		int address=startAddress;
		/* EEPROM write is in 8 byte or less chunks. After writing, we must poll for ack to know that write is done */
		do {
			int bytesWeCanWrite = 8-(address%8);

//			printf("# start of do {} loop: address=%d bytesWeCanWrite=%d\n",address,bytesWeCanWrite);

			/* read up to bytesWeCanWrite bytes + save one bytes for address */
			for ( i=1 ; i<(bytesWeCanWrite+1) && bytesRead < nBytes ; i++,bytesRead++ ) {
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
#if 0			
			fprintf(stderr,"# i=%d nBytes=%d bytesRead=%d done=%d\n",i,nBytes,bytesRead,done);
			int j;
			for ( j=0 ; j<i ; j++ ) {
				fprintf(stderr,"# txBuffer[%d] 0x%02X",j,txBuffer[j]);
				if ( 0 == j )
					fprintf(stderr," <- ADDRESS MSB ");
				if ( 1 == j )
					fprintf(stderr," <- ADDRESS LSB ");
				fprintf(stderr,"\n");
			}
#endif


			/* address low byte */
			txBuffer[0] = (address & 0b11111111);

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

			/* address low byte */
			txBuffer[0] = (nullAddress & 0b11111111);
			/* null byte */
			txBuffer[1] = '\0';

			/* write */
			opResult = write(i2cHandle, txBuffer, 2);

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
	if ( macRead ) {

		txBuffer[0] = 0xfa;

		/* write read address */
		opResult = write(i2cHandle, txBuffer, 1);
		if (opResult != 1) {
			fprintf(stderr,"# No ACK! Exiting...\n");
			exit(2);
		}

		/*  read 6 bytes */
		memset(rxBuffer, 0, 6);
		opResult = read(i2cHandle, rxBuffer, 6);

		fprintf(stderr,"# MAC address\n");
		printf("%02x:%02x:%02x:%02x:%02x:%02x\n",rxBuffer[0],rxBuffer[1],rxBuffer[2],rxBuffer[3],rxBuffer[4],rxBuffer[5]);
	} else if ( dumpRead ) {

		/* address low byte */
		txBuffer[0] = 0x00;

		/* write read address */
		opResult = write(i2cHandle, txBuffer, 1);
		if (opResult != 1) {
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

		/* address low byte */
		txBuffer[0] = (startAddress & 0b11111111);

		/* write read address */
		opResult = write(i2cHandle, txBuffer, 1);
		if (opResult != 1) {
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


	if ( -1 == close(i2cHandle) ) {
		fprintf(stderr,"# Error closing I2C device.\n# %s\n# Exiting...\n",strerror(errno));
		exit(1);
	}
	
	fprintf(stderr,"# Done...\n");
	
	exit(0);
}

