#ifndef APRSi2C_SENSORS_IMU_SENSOR_LSM9DS1_H
#define APRSi2C_SENSORS_IMU_SENSOR_LSM9DS1_H
void LSM9DS1_init(int, int);
void LSM9DS1_sample(int, int);
extern struct json_object *jobj_sensors_LSM9DS1,*jobj_sensors_LSM9DS1_gyro,*jobj_sensors_LSM9DS1_accel,*jobj_sensors_LSM9DS1_magnet;
extern struct json_object *jobj_sensors_LSM9DS1_gyro_array,*jobj_sensors_LSM9DS1_accel_array,*jobj_sensors_LSM9DS1_magnet_array;
#endif
