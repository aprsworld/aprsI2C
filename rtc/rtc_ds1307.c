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


/* byte BCD decoder */
uint8_t bcd2bin_uint8(uint8_t bcd_value) {
	return (10*(bcd_value>>4)) + (bcd_value&0x0f);
}

/* byte BCD encoder */
uint8_t bin2bcd_uint8(uint8_t bin_value) {
	return (((bin_value / 10) << 4) | (bin_value % 10));
}

void ds1307_build_date(char *buff, int year, int month, int day, int hour, int minute, int second) {

	/* seconds (0 to 59) */
	buff[0x00] = bin2bcd_uint8(second) & 0b01111111; /* enable oscillator and BCD seconds */

	/* minutes (0 to 59) */
	buff[0x01] = bin2bcd_uint8(minute);

	/* hours (0 to 23) in 24 hour format */
	buff[0x02] = bin2bcd_uint8(hour) & 0b00111111; /* 24  hour mode by making sure bit 6 is cleared */

	/* day of week ... we don't use and are just settings to 0 */
	buff[0x03] = 0x00;

	/* day (1 to 31) (DS1307 refers to this register as date)  */
	buff[0x04] = bin2bcd_uint8(day);

	/* month (1 to 12) */
	buff[0x05] = bin2bcd_uint8(month);

	/* year (0 to 99) */
	year = year % 100; 
	buff[0x06] = bin2bcd_uint8(year);

	/* control */
	buff[0x07] = 0x00; /* output state is low, square wave is not enabled */
}

int ds1307_build_date_from_string(char *iso8601, char *ds1307) {
	int year, month, day, hour, minute, second;
	int scanned;

	/* break ISO8601 format into components */
	scanned=sscanf(iso8601,"%d-%d-%d %d:%d:%d",&year,&month,&day,&hour,&minute,&second);

	/* range check date components */
	if ( month<1 || month>12 )
		return -1;
	if ( day<1 || day>31 )
		return -2;
	if ( hour<0 || hour>23 )
		return -3;
	if ( minute<0 || minute>59 )
		return -4;
	if ( second<0 || second>59 ) 
		return -5;

	if ( 6 != scanned ) {
//		fprintf(stderr,"# sscanf error while reading ISO 8601 like date. %s. Aborting setting date.\n",strerror(errno));
		return scanned;
	}

	/* pass to DS1307 encoder */
	ds1307_build_date(ds1307,year,month,day,hour,minute,second);

	return 0;
}

/* DS1307 registers 0 to 6, string to put YYYY-MM-DD HH:MM:SS result in, max length of result */
void ds1307_date_to_str(char *ds1307, char *s, int len) {
	int hour, minute, second, day, month, year;

	hour   = bcd2bin_uint8(ds1307[0x02] & 0b00111111);
	minute = bcd2bin_uint8(ds1307[0x01]);
	second = bcd2bin_uint8(ds1307[0x00] & 0b01111111);
	day    = bcd2bin_uint8(ds1307[0x04]);
	month  = bcd2bin_uint8(ds1307[0x05]);
	year   = bcd2bin_uint8(ds1307[0x06]) + 2000;

	snprintf(s,len,"%04d-%02d-%02d %02d:%02d:%02d",year,month,day,hour,minute,second);
}

int main(int argc, char **argv) {
	/* optarg */
	int c;
	int digit_optind = 0;

	/* program flow */
	char ramSetBuffer[57];
	char dateSetBuffer[20]; 
	int ramRead, dateRead, dumpRead; 

	/* I2C stuff */
	char i2cDevice[64];	/* I2C device name */
	int i2cAddress; 	/* chip address */


	char rxBuffer[64];	/* receive buffer */
	char txBuffer[64+1];	/* transmit buffer (extra byte is address byte) */
	int opResult = 0;	/* for error checking of operations */

	/* RTC stuff */
	int i;
	int seconds, minutes, hours, day, month, year;


	fprintf(stderr,"# rtc_ds1307 DS1307 Real Time Clock I2C utility\n");

	/* defaults and command line arguments */
	memset(ramSetBuffer, 0, sizeof(ramSetBuffer));
	memset(dateSetBuffer, 0, sizeof(dateSetBuffer));
	ramRead=dateRead=dumpRead=0;
	strcpy(i2cDevice,"/dev/i2c-1"); /* Raspberry PI normal user accessible I2C bus */
	i2cAddress=0x68; 		/* DS1307 is always hard-coded to 0x68 */

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"read",        no_argument,       0, 'r' },
		        {"ram-read",    no_argument,       0, 'R' },
		        {"set",         required_argument, 0, 's' }, 
		        {"ram-set",     required_argument, 0, 'S' },
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
			case 'S':
				strncpy(ramSetBuffer,optarg,sizeof(ramSetBuffer)-1);
				ramSetBuffer[sizeof(ramSetBuffer)-1]='\0';
				break;
			case 's':
				strncpy(dateSetBuffer,optarg,sizeof(dateSetBuffer)-1);
				dateSetBuffer[sizeof(dateSetBuffer)-1]='\0';
				break;
			case 'd':
				dumpRead=1;
				break;
			case 'R':
				ramRead=1;
				break;
			case 'r':
				dateRead=1;
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


	/* do any requested reading after sets */
	if ( dateRead || ramRead || dumpRead ) {

		/* address to read from */
		txBuffer[0] = 0x00; 

		/* write read address */
		opResult = write(i2cHandle, txBuffer, 1);
		if (opResult != 1) {
			fprintf(stderr,"# No ACK bit! Exiting...\n");
			exit(2);
		}

		/*  read buffer length */
		memset(rxBuffer, 0, sizeof(rxBuffer));
		opResult = read(i2cHandle, rxBuffer, sizeof(rxBuffer));

		if ( dateRead ) {
			fprintf(stderr,"# Date read from DS1307 RTC\n");
			ds1307_date_to_str(rxBuffer, txBuffer, sizeof(txBuffer));
			printf("%s\n",txBuffer);
		} else if ( ramRead ) {
			fprintf(stderr,"# RAM read from DS1307 RTC\n");
			for ( i=8 ; i<64 && rxBuffer[i] != '\0' ; i++ ) {
				putchar(rxBuffer[i]);
			}
		} else if ( dumpRead) {
			fprintf(stderr,"# Dump from DS1307 RTC\n");
			for ( i=0 ; i<64 ; i++ ) {
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

