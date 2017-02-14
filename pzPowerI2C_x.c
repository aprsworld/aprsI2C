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
	char rxBuffer[32];	// receive buffer
	char txBuffer[32];	// transmit buffer
	int pzPowerAddress = 0x50; // I2C device address
	int opResult = 0;	 // for error checking of operations
	int i;

	// Create a file descriptor for the I2C bus
	int i2cHandle = open("/dev/i2c-1", O_RDWR);

	// not using 10 bit addresses
	opResult = ioctl(i2cHandle, I2C_TENBIT, 0);

	// Tell the I2C peripheral what the address of the device is. 
	opResult = ioctl(i2cHandle, I2C_SLAVE, pzPowerAddress);

	// Clear our buffers
	memset(rxBuffer, 0, sizeof(rxBuffer));
	memset(txBuffer, 0, sizeof(txBuffer));

	/* address to read from */
	txBuffer[0] = 0x00; // This is the address we want to read from.
	opResult = write(i2cHandle, txBuffer, 1);
	if (opResult != 1) 
		printf("No ACK bit!\n");

	/*  read 16 bytes */
	opResult = read(i2cHandle, rxBuffer, 16);
	/* dump */
	for ( i=0 ; i<16 ; i++ ) {
		printf("# rxBuffer[%d]=0x%02x (%c)\n",i,rxBuffer[i],rxBuffer[i]);
	}

	/* write a string to pzPower */
	strcpy(txBuffer,"0hello, pzPower\0");
//                        0123456789012345

	/* address to write to */
	txBuffer[0] = 0x00; 
	/* dump */
	for ( i=0 ; i<16 ; i++ ) {
		printf("# txBuffer[%d]=0x%02x (%c)\n",i,txBuffer[i],txBuffer[i]);
	}
	opResult = write(i2cHandle, txBuffer, 16);
	if (opResult != 1) 
		printf("No ACK bit!\n");

}

