#ifndef APRSi2C_SENSORS_IMU_SENSOR_BMP280_H
#define APRSi2C_SENSORS_IMU_SENSOR_BMP280_H
extern void bmp280_init(int, int);
extern void bmp280_sample(int, int);
extern struct json_object *jobj_sensors_bmp280,*jobj_sensors_bmp280_array;
#endif

