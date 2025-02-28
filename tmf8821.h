#ifndef TMF8821_H
#define TMF8821_H


#include"i2c_usr.h"


#define ENABLE_REG 0xE0
#define CMD_STAT_REG 0x08
#define APPID_REG 0x00
#define INT_ENAB_REG 0xE2
#define INT_CLEAR_REG 0xE1
#define COMMON_CONFIG_REG 0x16
#define MEASURE_CMD 0x10
#define STOP_CMD 0x11
#define CONFIG_RESULT_REG 0x20

uint8_t calculate_checksum(uint8_t cmd_stat, uint8_t size, uint8_t *data, uint8_t data_length);
void download_init();
void set_address(uint16_t address);
void write_ram(uint8_t *data, uint8_t data_length);
void ram_remap_reset();
void check_device_ready();
void load_common_config();
void set_measurement_period();
void set_spad_mask();
void write_common_config();
void enable_interrupts();
void clear_interrupts();
void start_measurement();
void stop_measurement();
void read_measurement_results(uint16_t* res);
void check_cmd();
void check_conf();

#endif