#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	// Set up some variables that we'll use along the way
	char rxBuffer[64];	// receive buffer
	char txBuffer[64+1];	// transmit buffer (extra byte is address byte)
	int ds1307Addresss = 0x68; // I2C device address
	int opResult = 0;	 // for error checking of operations
	int i;

	// Create a file descriptor for the I2C bus
	int i2cHandle = open("/dev/i2c-1", O_RDWR);

	// not using 10 bit addresses
	opResult = ioctl(i2cHandle, I2C_TENBIT, 0);

	// Tell the I2C peripheral what the address of the device is. 
	opResult = ioctl(i2cHandle, I2C_SLAVE, ds1307Addresss);

	// Clear our buffers
	memset(rxBuffer, 0, sizeof(rxBuffer));
	memset(txBuffer, 0, sizeof(txBuffer));

	/* address to read from */
	txBuffer[0] = 0x00; // This is the address we want to read from.
	opResult = write(i2cHandle, txBuffer, 1);
	if (opResult != 1) 
		printf("No ACK bit!\n");

	/*  read 16 bytes */
	opResult = read(i2cHandle, rxBuffer, sizeof(rxBuffer));
	/* dump */
	for ( i=0 ; i<sizeof(rxBuffer) ; i++ ) {
		printf("# rxBuffer[%d]=0x%02X ",i,rxBuffer[i],rxBuffer[i]);
		if ( rxBuffer[i] >= 32 && rxBuffer[i] <= 127 ) {
			printf("(%c) ",rxBuffer[i]);
		} else {
			printf("(non-printable) ");
		}
		printf("\n");
	}

	/* write a string to pzPower */
	memset(txBuffer,'\0',sizeof(txBuffer));

	/* first byte will be replaced by address to write to */
	strcpy(txBuffer," hello, DS1307 RAM");

	/* address to write to */
	txBuffer[0] = 0x08; 
	/* dump */
	for ( i=0 ; i<sizeof(txBuffer) ; i++ ) {
		printf("# txBuffer[%d]=0x%02X ",i,txBuffer[i],txBuffer[i]);
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

