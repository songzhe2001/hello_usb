#include "i2c_usr.h"
#include "hardware/i2c.h"

// I²C初始化
void i2c_init_bus()
{
    i2c_init(I2C_PORT, 80000);            // 初始化I²C，波特率100kHz
    gpio_set_function(16, GPIO_FUNC_I2C); // SDA引脚
    gpio_set_function(17, GPIO_FUNC_I2C); // SCL引脚
    gpio_pull_up(16);                     // 上拉SDA
    gpio_pull_up(17);                     // 上拉SCL
}

// I²C写一个字节
void i2c_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 2, false);
}

// I²C读一个字节
uint8_t i2c_read_byte(uint8_t reg)
{
    uint8_t data;
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &reg, 1, true);  // 发送寄存器地址
    i2c_read_blocking(I2C_PORT, I2C_ADDRESS, &data, 1, false); // 读取数据
    return data;
}

// I²C读多个字节
void i2c_read_bytes(uint8_t reg, uint8_t *data, uint8_t size)
{
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &reg, 1, true);    // 发送寄存器地址
    i2c_read_blocking(I2C_PORT, I2C_ADDRESS, data, size, false); // 读取数据
}
