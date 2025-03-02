#include "tmf8821.h"
#include "hardware/i2c.h"
#include "i2c_usr.h"

// 计算校验
uint8_t calculate_checksum(uint8_t cmd_stat, uint8_t size, uint8_t *data, uint8_t data_length)
{
    uint16_t sum = cmd_stat + size;
    for (int i = 0; i < data_length; i++)
    {
        sum += data[i];
    }
    return ~(sum & 0xFF); // 取最低字节的补码
}

// 下载初始化
void download_init()
{
    uint8_t cmd_stat = 0x14; // DOWNLOAD_INIT命令
    uint8_t size = 0x01;
    uint8_t data = 0x29;
    uint8_t checksum = calculate_checksum(cmd_stat, size, &data, 1);

    uint8_t buf[5] = {CMD_STAT_REG, cmd_stat, size, data, checksum};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 5, false);

    printf("DOWNLOAD_INIT command sent.\n");
}

// 设置目标地址
void set_address(uint16_t address)
{
    uint8_t cmd_stat = 0x43; // SET_ADDR命令
    uint8_t size = 0x02;
    uint8_t data[2] = {(uint8_t)(address >> 8), (uint8_t)(address & 0xFF)};
    uint8_t checksum = calculate_checksum(cmd_stat, size, data, 2);

    uint8_t buf[6] = {CMD_STAT_REG, cmd_stat, size, data[0], data[1], checksum};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 6, false);

    printf("SET_ADDR command sent for address 0x%04X.\n", address);
}

// 写入RAM数据
void write_ram(uint8_t *data, uint8_t data_length)
{
    uint8_t cmd_stat = 0x41; // W_RAM命令
    uint8_t size = data_length;
    uint8_t checksum = calculate_checksum(cmd_stat, size, data, data_length);

    uint8_t buf[4 + data_length];
    buf[0] = CMD_STAT_REG;
    buf[1] = cmd_stat;
    buf[2] = size;
    for (int i = 0; i < data_length; i++)
    {
        buf[3 + i] = data[i];
    }
    buf[3 + data_length] = checksum;
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 3 + data_length + 1, false);
}

// 完成下载并重启设备
void ram_remap_reset()
{
    uint8_t cmd_stat = 0x11; // RAMREMAP_RESET命令
    uint8_t size = 0x00;
    uint8_t checksum = calculate_checksum(cmd_stat, size, NULL, 0);

    uint8_t buf[4] = {CMD_STAT_REG, cmd_stat, size, checksum};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 4, false);

    printf("RAMREMAP_RESET command sent.\n");
}

// 检查设备是否准备好通信
void check_device_ready()
{
    uint8_t enable_value;

    while (1)
    {
        enable_value = i2c_read_byte(ENABLE_REG); // 读取ENABLE寄存器的值
        // 检查设备状态
        if ((enable_value & 0x41) == 0x41)
        {
            // 设备已准备好通信 (bit 6 = 1)
            printf("Device is ready for communication.\n");
            printf("ENABLE register value: 0x%02X\n", enable_value);

            break;
        }
        else if ((enable_value & 0x41) == 0x01)
        {
            // CPU正在初始化 (bit 6 = 0, bit 1 = 1)
            printf("CPU is initializing. Polling until ready...\n");
            sleep_ms(10); // 等待10ms
        }
        else if ((enable_value & 0x06) == 0x02)
        {
            // 设备处于STANDBY模式 (bit 6 = 0, bit 1 = 0, bit 2 = 1)
            printf("Device is in STANDBY mode. Waking up...\n");
            uint8_t new_value = (enable_value & 0x30) | 0x01; // 保持bit 5和4，设置bit 0为1
            i2c_write_byte(ENABLE_REG, new_value);            // 写入新值唤醒设备
            sleep_ms(10);                                     // 等待10ms
        }
        else if ((enable_value & 0x06) == 0x06)
        {
            // 设备处于STANDBY_TIMED模式 (bit 6 = 0, bit 1 = 1, bit 2 = 1)
            printf("Device is in STANDBY_TIMED mode. Forcing wake up...\n");
            uint8_t new_value = (enable_value & 0x30) | 0x01; // 保持bit 5和4，设置bit 0为1
            i2c_write_byte(ENABLE_REG, new_value);            // 写入新值强制唤醒设备
            sleep_ms(10);                                     // 等待10ms
        }
        else
        {
            // 未知状态
            printf("Unexpected ENABLE register value: 0x%02X\n", enable_value);
            break;
        }
    }
}

// 加载公共配置页面
void load_common_config()
{
    i2c_write_byte(CMD_STAT_REG, COMMON_CONFIG_REG); // 加载公共配置页面
    while (i2c_read_byte(CMD_STAT_REG) != 0x00)
    { // 等待命令完成
        sleep_ms(10);
    }
    printf("Common configuration page loaded.\n");
}

// 配置测量周期为100ms
void set_measurement_period()
{
    i2c_write_byte(0x24, 0x50); // 设置测量周期为100ms
    i2c_write_byte(0x25, 0x00); // 高位字节
    printf("Measurement period set to 100ms.\n");
}

// 选择SPAD掩码6
void set_spad_mask()
{
    i2c_write_byte(0x34, 0x06); // 选择SPAD掩码6
    printf("SPAD mask set to 6.\n");
}

// 写入公共配置页面
void write_common_config()
{
    i2c_write_byte(CMD_STAT_REG, 0x15); // 写入公共配置页面
    while (i2c_read_byte(CMD_STAT_REG) != 0x00)
    { // 等待命令完成
        sleep_ms(10);
    }
    printf("Common configuration page written.\n");
}

// 启用结果中断
void enable_interrupts()
{
    i2c_write_byte(INT_ENAB_REG, 0x02); // 启用结果中断
    printf("Result interrupts enabled.\n");
}

// 清除中断
void clear_interrupts()
{
    i2c_write_byte(INT_CLEAR_REG, 0xFF); // 清除所有中断
    printf("Interrupts cleared.\n");
}

// 启动测量
void start_measurement()
{
    i2c_write_byte(CMD_STAT_REG, MEASURE_CMD); // 启动测量
    while (i2c_read_byte(CMD_STAT_REG) != 0x01)
    { // 等待命令完成
        sleep_ms(10);
    }
    printf("Measurement started.\n");
}

// 停止测量
void stop_measurement()
{
    i2c_write_byte(CMD_STAT_REG, STOP_CMD); // 停止测量
    while (i2c_read_byte(CMD_STAT_REG) != 0x00)
    { // 等待命令完成
        sleep_ms(10);
    }
    printf("Measurement stopped.\n");
}

// 读取测量结果
void read_measurement_results(uint16_t *res)
{
    uint8_t result_id = i2c_read_byte(0x20); // 读取结果ID
    if (result_id == 0x10)
    { // 检查是否为测量结果
        uint8_t conf = 0;
        uint8_t dm;
        uint8_t dl;
        uint8_t i = 0x38;
        uint8_t j = 0;
        // uint8_t k = 0;
        // dl = i2c_read_byte(0x3C);
        //     dm = i2c_read_byte(0x3D);
        //     printf("value: 0x%02X%02X    ", dm, dl);
        while (1)
        {
            res[j] = i2c_read_byte(i+j);
            j++;
            if(j>=27)
                break;
        }
    }
    else
    {
        printf("Unexpected result ID: 0x%02X\n", result_id);
    }
}

void check_cmd()
{
    uint8_t data[3];
    while (true)
    {
        sleep_ms(1);
        i2c_read_bytes(CMD_STAT_REG, data, 3);
        printf("ready register value: 0x%02X 0x%02X 0x%02X\n", data[0], data[1], data[2]);
        if (data[2] == 0xff)
            break;
        sleep_ms(2);
    }
}

void check_conf()
{
    uint8_t data[3];
    while (true)
    {
        sleep_ms(1);
        i2c_read_bytes(CONFIG_RESULT_REG, data, 4);
        printf("Check configuration register value: 0x%02X 0x%02X 0x%02X 0x%02X\n", data[0], data[1], data[2], data[3]);
        if ((data[0] == 0x16) && (data[2] == 0xbc) && (data[3] == 0x00))
            break;
        sleep_ms(2);
    }
}