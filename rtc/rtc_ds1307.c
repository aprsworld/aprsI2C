#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

extern char *optarg;
extern int optind, opterr, optopt;

/* 
Date format used is ISO 8601 style:
yyyy-MM-dd HH:mm:ss


Unix date command to print in this format:
date +"%Y-%m-%d %k:%M:%S"

Unix date command to set in this format:
date --set "2017-02-14 17:27:09"
*/

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
	buff[0x00] = 0b10000000 | bin2bcd_uint8(second); /* enable oscillator and BCD seconds */

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

void ds1307_built_date_from_string(char *iso8601, char *ds1307) {
	int year, month, day, hour, minute, second;

	/* break ISO8601 format into components */
	sscanf(iso8601,"%d-%d-%d %d:%d:%d",&year,&month,&day,&hour,&minute,&second);

	/* pass to DS1307 encoder */
	ds1307_build_date(ds1307,year,month,day,hour,minute,second);
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

int main(void) {
	/* I2C stuff */
	char rxBuffer[64];	// receive buffer
	char txBuffer[64+1];	// transmit buffer (extra byte is address byte)
	int ds1307Addresss = 0x68; // I2C device address
	int opResult = 0;	 // for error checking of operations

	/* RTC stuff */
	int i;
	int seconds, minutes, hours, day, month, year;

	/* Open I2C bus */
	int i2cHandle = open("/dev/i2c-1", O_RDWR);

	/* not using 10 bit addresses */
	opResult = ioctl(i2cHandle, I2C_TENBIT, 0);

	/* address of device we will be working with */
	opResult = ioctl(i2cHandle, I2C_SLAVE, ds1307Addresss);

	/* address to read from */
	txBuffer[0] = 0x00; 

	/* write read address */
	opResult = write(i2cHandle, txBuffer, 1);
	if (opResult != 1) 
		printf("No ACK bit!\n");

	/*  read buffer length */
	memset(rxBuffer, 0, sizeof(rxBuffer));
	opResult = read(i2cHandle, rxBuffer, sizeof(rxBuffer));
	/* dump */
	for ( i=0 ; i<sizeof(rxBuffer) ; i++ ) {
		printf("# rxBuffer[%-2d]=0x%02X ",i,rxBuffer[i],rxBuffer[i]);
		if ( rxBuffer[i] >= 32 && rxBuffer[i] <= 127 ) {
			printf("(%c) ",rxBuffer[i]);
		} else {
			printf("(non-printable) ");
		}
		printf("\n");
	}


	/* write a string to scratch RAM */
	memset(txBuffer,'\0',sizeof(txBuffer));
	/* first byte will be replaced by address to write to */
//	strcpy(txBuffer," hello, DS1307 RAM");

	ds1307_date_to_str(rxBuffer, txBuffer+1, sizeof(txBuffer)-1 );

	/* address to write to */
	txBuffer[0] = 0x08; 
	/* dump */
	for ( i=0 ; i<sizeof(txBuffer) ; i++ ) {
		printf("# txBuffer[%-2d]=0x%02X ",i,txBuffer[i],txBuffer[i]);
		if ( txBuffer[i] >= 32 && txBuffer[i] <= 127 ) {
			printf("(%c) ",txBuffer[i]);
		} else {
			printf("(non-printable) ");
		}
		printf("\n");
	}

	/* write 56 bytes to it */
	opResult = write(i2cHandle, txBuffer, 56);
	if (opResult != 1) 
		printf("No ACK bit!\n");

	return 0;

}

