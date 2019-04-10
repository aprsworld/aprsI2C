nt16 data_to_send=18100; //In the next step of the code development this data is going to change every loop (sensor data) 
int16 data_sent; 
int16 data_received; 

#int_SSP 
void i2c_interrupt() { 
	int state; 
	int writeBuffer[2]; 
	int readBuffer[2]; 

	state = i2c_isr_state(); 

	if ( state==0 ) { 
		//Address match received with R/W bit clear, perform i2c_read( ) to read the I2C address. 
		i2c_read(); 
	} else if ( state==0x80 ) {
		//Address match received with R/W bit set; perform i2c_read( ) to read the I2C address, 
		//and use i2c_write( ) to pre-load the transmit buffer for the next transaction (next I2C read performed by master will read this byte). 
		i2c_read(2); 
	}

	if ( state>=0x80 ) {
		//Master is waiting for data 
		writeBuffer[0] = (data_to_send & 0xFF); //LSB first 
		writeBuffer[1] = ((data_to_send & 0xFF00)>>8);  //MSB secound 

		i2c_write(writeBuffer[state - 0x80]); //Write appropriate byte, based on how many have already been written 

		if ((state-0x80)==2) { 
			data_sent=make16(writeBuffer[1],writeBuffer[0]); 
			printf("\nFull data sent: %ld\n",data_sent); 
		} 
	} else if ( state>0 ) {
		//Master has sent data; read. 

		readBuffer[state - 1] = i2c_read(); //LSB first and MSB secound 

		if (state==2) { 
			data_received=make16(readBuffer[1],readBuffer[0]); 
			printf("\nFull data received: %ld\n",data_received); 
		} 
	} 
} 
