#ifndef _DHT_READ_DATA_
#define _DHT_READ_DATA_
#include "driver/gpio.h"
uint8_t dht_read_data(gpio_num_t pin, double *humidity, double *temperature);
#endif