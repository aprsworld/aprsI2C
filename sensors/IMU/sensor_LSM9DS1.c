/*
    This program  reads the angles from the accelerometer and gyroscope
    on a BerryIMU connected to a Raspberry Pi.


    Both the BerryIMUv1 and BerryIMUv2 are supported

    Feel free to do whatever you like with this code
    Distributed as-is; no warranty is given.

    http://ozzmaker.com/
*/




#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <json.h>
#include <math.h>
#include "i2c-dev.h" /* TODO: this should be using linux/i2c.h and/or linux/i2c-dev.h ... BerryIMU file seems to be weird hybrid */
#include "LSM9DS0.h"
#include "LSM9DS1.h"

static int file;
static int LSM9DS0 = 0;
static int LSM9DS1 = 0;

#define DT 0.02         // [s/loop] loop period. 20ms
#define AA 0.97         // complementary filter constant

#define A_GAIN 0.0573    // [deg/LSB]
#define G_GAIN 0.070     // [deg/s/LSB]
#define RAD_TO_DEG 57.29578
#define M_PI 3.14159265358979323846

#if 0
int outputDebug=0;
#endif

/* JSON stuff */
struct json_object *jobj_sensors_LSM9DS1,*jobj_sensors_LSM9DS1_gyro,*jobj_sensors_LSM9DS1_accel,*jobj_sensors_LSM9DS1_magnet;

void  readBlock(uint8_t command, uint8_t size, uint8_t *data)
{
    int result = i2c_smbus_read_i2c_block_data(file, command, size, data);
    if (result != size){
		printf("Failed to read block from I2C.");
		exit(1);
	}
}

void selectDevice(int file, int addr)
{
	if (ioctl(file, I2C_SLAVE, addr) < 0) {
		 printf("Failed to select I2C device.");
	}
}


void readACC(int  *a)
{
	uint8_t block[6];
	if (LSM9DS0){
		selectDevice(file,LSM9DS0_ACC_ADDRESS);
		readBlock(0x80 |  LSM9DS0_OUT_X_L_A, sizeof(block), block);
	}
	else if (LSM9DS1){
		selectDevice(file,LSM9DS1_ACC_ADDRESS);
		readBlock(0x80 |  LSM9DS1_OUT_X_L_XL, sizeof(block), block);       
	}
	
	// Combine readings for each axis.
	*a = (int16_t)(block[0] | block[1] << 8);
	*(a+1) = (int16_t)(block[2] | block[3] << 8);
	*(a+2) = (int16_t)(block[4] | block[5] << 8);

}


void readMAG(int  *m)
{
	uint8_t block[6];
    if (LSM9DS0){
		selectDevice(file,LSM9DS0_MAG_ADDRESS);
		readBlock(0x80 |  LSM9DS0_OUT_X_L_M, sizeof(block), block);
	}
	else if (LSM9DS1){
		selectDevice(file,LSM9DS1_MAG_ADDRESS);
		readBlock(0x80 |  LSM9DS1_OUT_X_L_M, sizeof(block), block);    
	}
	
	// Combine readings for each axis.
	*m = (int16_t)(block[0] | block[1] << 8);
	*(m+1) = (int16_t)(block[2] | block[3] << 8);
	*(m+2) = (int16_t)(block[4] | block[5] << 8);

}

void readGYR(int *g)
{
	uint8_t block[6];
    if (LSM9DS0){
		selectDevice(file,LSM9DS0_GYR_ADDRESS);
		readBlock(0x80 |  LSM9DS0_OUT_X_L_G, sizeof(block), block);
	}
	else if (LSM9DS1){
		selectDevice(file,LSM9DS1_GYR_ADDRESS);
		readBlock(0x80 |  LSM9DS1_OUT_X_L_G, sizeof(block), block);    
	}
 

	// Combine readings for each axis.
	*g = (int16_t)(block[0] | block[1] << 8);
	*(g+1) = (int16_t)(block[2] | block[3] << 8);
	*(g+2) = (int16_t)(block[4] | block[5] << 8);
}


void writeAccReg(uint8_t reg, uint8_t value)
{
	if (LSM9DS0)
		selectDevice(file,LSM9DS0_ACC_ADDRESS);
	else if (LSM9DS1)
		selectDevice(file,LSM9DS1_ACC_ADDRESS);
	
	int result = i2c_smbus_write_byte_data(file, reg, value);
	if (result == -1){
		printf ("Failed to write byte to I2C Acc.");
        exit(1);
    }
}

void writeMagReg(uint8_t reg, uint8_t value)
{
    if (LSM9DS0)
		selectDevice(file,LSM9DS0_MAG_ADDRESS);
	else if (LSM9DS1)
		selectDevice(file,LSM9DS1_MAG_ADDRESS);
  
	int result = i2c_smbus_write_byte_data(file, reg, value);
	if (result == -1){
		printf("Failed to write byte to I2C Mag.");
		exit(1);
	}
}


void writeGyrReg(uint8_t reg, uint8_t value)
{
    if (LSM9DS0)
		selectDevice(file,LSM9DS0_GYR_ADDRESS);
	else if (LSM9DS1)
		selectDevice(file,LSM9DS1_GYR_ADDRESS);
  
	int result = i2c_smbus_write_byte_data(file, reg, value);
	if (result == -1){
		printf("Failed to write byte to I2C Gyr.");
		exit(1);
	}
}



void detectIMU(void)
{

	__u16 block[I2C_SMBUS_BLOCK_MAX];

	int res, bus,  size;


	char filename[20];
	sprintf(filename, "/dev/i2c-%d", 1);
	file = open(filename, O_RDWR);
	if (file<0) {
		printf("Unable to open I2C bus!");
			exit(1);
	}

	//Detect if BerryIMUv1 (Which uses a LSM9DS0) is connected
	selectDevice(file,LSM9DS0_ACC_ADDRESS);
	int LSM9DS0_WHO_XM_response = i2c_smbus_read_byte_data(file, LSM9DS0_WHO_AM_I_XM);

	selectDevice(file,LSM9DS0_GYR_ADDRESS);	
	int LSM9DS0_WHO_G_response = i2c_smbus_read_byte_data(file, LSM9DS0_WHO_AM_I_G);

	if (LSM9DS0_WHO_G_response == 0xd4 && LSM9DS0_WHO_XM_response == 0x49){
		printf ("\n\n\n#####   BerryIMUv1/LSM9DS0  DETECTED    #####\n\n");
		LSM9DS0 = 1;
	}




	//Detect if BerryIMUv2 (Which uses a LSM9DS1) is connected
	selectDevice(file,LSM9DS1_MAG_ADDRESS);
	int LSM9DS1_WHO_M_response = i2c_smbus_read_byte_data(file, LSM9DS1_WHO_AM_I_M);

	selectDevice(file,LSM9DS1_GYR_ADDRESS);	
	int LSM9DS1_WHO_XG_response = i2c_smbus_read_byte_data(file, LSM9DS1_WHO_AM_I_XG);

    if (LSM9DS1_WHO_XG_response == 0x68 && LSM9DS1_WHO_M_response == 0x3d){
		printf ("\n\n\n#####   BerryIMUv2/LSM9DS1  DETECTED    #####\n\n");
		LSM9DS1 = 1;
	}
  


	if (!LSM9DS0 && !LSM9DS1){
		printf ("NO IMU DETECTED\n");
		exit(1);
	}
}




void enableIMU(void)
{

	if (LSM9DS0){//For BerryIMUv1
		// Enable accelerometer.
		writeAccReg(LSM9DS0_CTRL_REG1_XM, 0b01100111); //  z,y,x axis enabled, continuous update,  100Hz data rate
		writeAccReg(LSM9DS0_CTRL_REG2_XM, 0b00100000); // +/- 16G full scale

		//Enable the magnetometer
		writeMagReg(LSM9DS0_CTRL_REG5_XM, 0b11110000); // Temp enable, M data rate = 50Hz
		writeMagReg(LSM9DS0_CTRL_REG6_XM, 0b01100000); // +/-12gauss
		writeMagReg(LSM9DS0_CTRL_REG7_XM, 0b00000000); // Continuous-conversion mode

		// Enable Gyro
		writeGyrReg(LSM9DS0_CTRL_REG1_G, 0b00001111); // Normal power mode, all axes enabled
		writeGyrReg(LSM9DS0_CTRL_REG4_G, 0b00110000); // Continuos update, 2000 dps full scale
	}

	if (LSM9DS1){//For BerryIMUv2      
		// Enable the gyroscope
		writeGyrReg(LSM9DS1_CTRL_REG4,0b00111000);      // z, y, x axis enabled for gyro
		writeGyrReg(LSM9DS1_CTRL_REG1_G,0b10111000);    // Gyro ODR = 476Hz, 2000 dps
		writeGyrReg(LSM9DS1_ORIENT_CFG_G,0b10111000);   // Swap orientation 

		// Enable the accelerometer
		writeAccReg(LSM9DS1_CTRL_REG5_XL,0b00111000);   // z, y, x axis enabled for accelerometer
		writeAccReg(LSM9DS1_CTRL_REG6_XL,0b00101000);   // +/- 16g

		//Enable the magnetometer
		writeMagReg(LSM9DS1_CTRL_REG1_M, 0b10011100);   // Temp compensation enabled,Low power mode mode,80Hz ODR
		writeMagReg(LSM9DS1_CTRL_REG2_M, 0b01000000);   // +/-12gauss
		writeMagReg(LSM9DS1_CTRL_REG3_M, 0b00000000);   // continuos update
		writeMagReg(LSM9DS1_CTRL_REG4_M, 0b00000000);   // lower power mode for Z axis
	}

}


void LSM9DS1_init(int i2cHandle, int i2cAddress) {

	detectIMU();
	enableIMU();
}

void LSM9DS1_sample(int i2cHandle, int i2cAddress) {
	char buffer[64];
        float accXnorm,accYnorm,pitch,roll,magXcomp,magYcomp;



	float rate_gyr_y = 0.0;   // [deg/s]
	float rate_gyr_x = 0.0;   // [deg/s]
	float rate_gyr_z = 0.0;   // [deg/s]

	int  accRaw[3];
	int  magRaw[3];
	int  gyrRaw[3];



	float gyroXangle = 0.0;
	float gyroYangle = 0.0;
	float gyroZangle = 0.0;
	float AccYangle = 0.0;
	float AccXangle = 0.0;
	float CFangleX = 0.0;
	float CFangleY = 0.0;


	//read MAG ACC and GYR data
	readACC(accRaw);
	readGYR(gyrRaw);
	readMAG(magRaw);


	//Convert Gyro raw to degrees per second
	rate_gyr_x = (float) gyrRaw[0] * G_GAIN;
	rate_gyr_y = (float) gyrRaw[1]  * G_GAIN;
	rate_gyr_z = (float) gyrRaw[2]  * G_GAIN;



	//Calculate the angles from the gyro
	gyroXangle+=rate_gyr_x*DT;
	gyroYangle+=rate_gyr_y*DT;
	gyroZangle+=rate_gyr_z*DT;




	//Convert Accelerometer values to degrees
	AccXangle = (float) (atan2(accRaw[1],accRaw[2])+M_PI)*RAD_TO_DEG;
	AccYangle = (float) (atan2(accRaw[2],accRaw[0])+M_PI)*RAD_TO_DEG;

	//Change the rotation value of the accelerometer to -/+ 180 and move the Y axis '0' point to up.
	//Two different pieces of code are used depending on how your IMU is mounted.
	//If IMU is upside down
	/*
		if (AccXangle >180)
				AccXangle -= (float)360.0;

		AccYangle-=90;
		if (AccYangle >180)
				AccYangle -= (float)360.0;
	*/

	//If IMU is up the correct way, use these lines
	AccXangle -= (float)180.0;
	if (AccYangle > 90)
			AccYangle -= (float)270;
	else
		AccYangle += (float)90;


	//Complementary filter used to combine the accelerometer and gyro values.
	CFangleX=AA*(CFangleX+rate_gyr_x*DT) +(1 - AA) * AccXangle;
	CFangleY=AA*(CFangleY+rate_gyr_y*DT) +(1 - AA) * AccYangle;


	//printf ("   GyroX  %7.3f \t AccXangle \e[m %7.3f \t \033[22;31mCFangleX %7.3f\033[0m\t GyroY  %7.3f \t AccYangle %7.3f \t \033[22;36mCFangleY %7.3f\t\033[0m\n",gyroXangle,AccXangle,CFangleX,gyroYangle,AccYangle,CFangleY);

	/* put data in JSON objects */
	/* put gyroscope data in jobj_sensors_LSM9DS1_gyro */
	snprintf(buffer,sizeof(buffer),"%1.3f",gyroXangle);
	json_object_object_add(jobj_sensors_LSM9DS1_gyro, "gyro_x", json_object_new_string(buffer));
	snprintf(buffer,sizeof(buffer),"%1.3f",gyroYangle);
	json_object_object_add(jobj_sensors_LSM9DS1_gyro, "gyro_y", json_object_new_string(buffer));
	snprintf(buffer,sizeof(buffer),"%1.3f",gyroZangle);
	json_object_object_add(jobj_sensors_LSM9DS1_gyro, "gyro_z", json_object_new_string(buffer));

	/* put accelerometer data in jobj_sensors_LSM9DS1_accel */
	snprintf(buffer,sizeof(buffer),"%1.3f",AccXangle);
	json_object_object_add(jobj_sensors_LSM9DS1_accel, "accel_x", json_object_new_string(buffer));
	snprintf(buffer,sizeof(buffer),"%1.3f",AccYangle);
	json_object_object_add(jobj_sensors_LSM9DS1_accel, "accel_y", json_object_new_string(buffer));

	//Compute heading
	float heading = 180 * atan2(magRaw[1],magRaw[0])/M_PI;

	//Convert heading to 0 - 360
	if(heading < 0)
		heading += 360;


	/* put magnetometer data in jobj_sensors_LSM9DS1_magnet */
	snprintf(buffer,sizeof(buffer),"%1.3f",heading);
	json_object_object_add(jobj_sensors_LSM9DS1_magnet, "magnet_heading", json_object_new_string(buffer));


	/* put gyro, accel, and magnet into main LSM9DS1 */
	json_object_object_add(jobj_sensors_LSM9DS1, "gyrometer", jobj_sensors_LSM9DS1_gyro);
	json_object_object_add(jobj_sensors_LSM9DS1, "accelerometer", jobj_sensors_LSM9DS1_accel);
	json_object_object_add(jobj_sensors_LSM9DS1, "magnetometer", jobj_sensors_LSM9DS1_magnet);
}
