#ifndef I2C_USR_H
#define I2C_USR_H

#include <stdio.h>
#include "pico/stdlib.h"

#define I2C_PORT i2c0
#define I2C_ADDRESS 0x41

void i2c_init_bus();
void i2c_write_byte(uint8_t reg, uint8_t data);
uint8_t i2c_read_byte(uint8_t reg);
void i2c_read_bytes(uint8_t reg, uint8_t *data, uint8_t size);

#endif