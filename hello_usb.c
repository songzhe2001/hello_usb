#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include"tmf882x_image.h"

#define I2C_PORT i2c0
#define I2C_ADDRESS 0x41
#define ENABLE_REG 0xE0
#define CMD_STAT_REG 0x08
#define APPID_REG 0x00
#define INT_ENAB_REG 0xE2
#define INT_CLEAR_REG 0xE1
#define COMMON_CONFIG_REG 0x16
#define MEASURE_CMD 0x10
#define STOP_CMD 0x11

extern const unsigned char tmf882x_image[];

// I²C初始化
void i2c_init_bus() {
    i2c_init(I2C_PORT, 40000);  // 初始化I²C，波特率100kHz
    gpio_set_function(16, GPIO_FUNC_I2C);  // SDA引脚
    gpio_set_function(17, GPIO_FUNC_I2C);  // SCL引脚
    gpio_pull_up(16);  // 上拉SDA
    gpio_pull_up(17);  // 上拉SCL
}

// I²C写一个字节
void i2c_write_byte(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 2, false);
}

// I²C读一个字节
uint8_t i2c_read_byte(uint8_t reg) {
    uint8_t data;
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &reg, 1, true);  // 发送寄存器地址
    i2c_read_blocking(I2C_PORT, I2C_ADDRESS, &data, 1, false); // 读取数据
    return data;
}

uint8_t calculate_checksum(uint8_t cmd_stat, uint8_t size, uint8_t *data, uint8_t data_length) {
    uint16_t sum = cmd_stat + size;
    for (int i = 0; i < data_length; i++) {
        sum += data[i];
    }
    return ~(sum & 0xFF);  // 取最低字节的补码
}

// 下载初始化
void download_init() {
    uint8_t cmd_stat = 0x14;  // DOWNLOAD_INIT命令
    uint8_t size = 0x01;
    uint8_t data = 0x29;
    uint8_t checksum = calculate_checksum(cmd_stat, size, &data, 1);

    uint8_t buf[5] = {CMD_STAT_REG, cmd_stat, size, data, checksum};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 5, false);

    printf("DOWNLOAD_INIT command sent.\n");
}

// 设置目标地址
void set_address(uint16_t address) {
    uint8_t cmd_stat = 0x43;  // SET_ADDR命令
    uint8_t size = 0x02;
    uint8_t data[2] = {(uint8_t)(address >> 8), (uint8_t)(address & 0xFF)};
    uint8_t checksum = calculate_checksum(cmd_stat, size, data, 2);

    uint8_t buf[6] = {CMD_STAT_REG, cmd_stat, size, data[0], data[1], checksum};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 6, false);

    printf("SET_ADDR command sent for address 0x%04X.\n", address);
}

// 写入RAM数据
void write_ram(uint8_t *data, uint8_t data_length) {
    uint8_t cmd_stat = 0x41;  // W_RAM命令
    uint8_t size = data_length;
    uint8_t checksum = calculate_checksum(cmd_stat, size, data, data_length);

    uint8_t buf[4 + data_length];
    buf[0] = CMD_STAT_REG;
    buf[1] = cmd_stat;
    buf[2] = size;
    for (int i = 0; i < data_length; i++) {
        buf[3 + i] = data[i];
    }
    buf[3 + data_length] = checksum;
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 3 + data_length + 1, false);
}

// 完成下载并重启设备
void ram_remap_reset() {
    uint8_t cmd_stat = 0x11;  // RAMREMAP_RESET命令
    uint8_t size = 0x00;
    uint8_t checksum = calculate_checksum(cmd_stat, size, NULL, 0);

    uint8_t buf[4] = {CMD_STAT_REG, cmd_stat, size, checksum};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buf, 4, false);

    printf("RAMREMAP_RESET command sent.\n");
}

// 检查设备是否准备好通信
void check_device_ready() {
    uint8_t enable_value;

    while (1) {
        enable_value = i2c_read_byte(ENABLE_REG);  // 读取ENABLE寄存器的值
        // 检查设备状态
        if ((enable_value & 0x41) == 0x41) {
            // 设备已准备好通信 (bit 6 = 1)
            printf("Device is ready for communication.\n");
            printf("ENABLE register value: 0x%02X\n", enable_value);

            break;
        } else if ((enable_value & 0x41) == 0x01) {
            // CPU正在初始化 (bit 6 = 0, bit 1 = 1)
            printf("CPU is initializing. Polling until ready...\n");
            sleep_ms(10);  // 等待10ms
        } else if ((enable_value & 0x06) == 0x02) {
            // 设备处于STANDBY模式 (bit 6 = 0, bit 1 = 0, bit 2 = 1)
            printf("Device is in STANDBY mode. Waking up...\n");
            uint8_t new_value = (enable_value & 0x30) | 0x01;  // 保持bit 5和4，设置bit 0为1
            i2c_write_byte(ENABLE_REG, new_value);  // 写入新值唤醒设备
            sleep_ms(10);  // 等待10ms
        } else if ((enable_value & 0x06) == 0x06) {
            // 设备处于STANDBY_TIMED模式 (bit 6 = 0, bit 1 = 1, bit 2 = 1)
            printf("Device is in STANDBY_TIMED mode. Forcing wake up...\n");
            uint8_t new_value = (enable_value & 0x30) | 0x01;  // 保持bit 5和4，设置bit 0为1
            i2c_write_byte(ENABLE_REG, new_value);  // 写入新值强制唤醒设备
            sleep_ms(10);  // 等待10ms
        } else {
            // 未知状态
            printf("Unexpected ENABLE register value: 0x%02X\n", enable_value);
            break;
        }
    }
}

// 加载公共配置页面
void load_common_config() {
    i2c_write_byte(CMD_STAT_REG, COMMON_CONFIG_REG);  // 加载公共配置页面
    while (i2c_read_byte(CMD_STAT_REG) != 0x00) {    // 等待命令完成
        sleep_ms(10);
    }
    printf("Common configuration page loaded.\n");
}

// 配置测量周期为100ms
void set_measurement_period() {
    i2c_write_byte(0x24, 0xc8);  // 设置测量周期为100ms
    i2c_write_byte(0x25, 0x00);  // 高位字节
    printf("Measurement period set to 100ms.\n");
}

// 选择SPAD掩码6
void set_spad_mask() {
    i2c_write_byte(0x34, 0x06);  // 选择SPAD掩码6
    printf("SPAD mask set to 6.\n");
}

// 写入公共配置页面
void write_common_config() {
    i2c_write_byte(CMD_STAT_REG, 0x15);  // 写入公共配置页面
    while (i2c_read_byte(CMD_STAT_REG) != 0x00) {  // 等待命令完成
        sleep_ms(10);
    }
    printf("Common configuration page written.\n");
}

// 启用结果中断
void enable_interrupts() {
    i2c_write_byte(INT_ENAB_REG, 0x02);  // 启用结果中断
    printf("Result interrupts enabled.\n");
}

// 清除中断
void clear_interrupts() {
    i2c_write_byte(INT_CLEAR_REG, 0xFF);  // 清除所有中断
    printf("Interrupts cleared.\n");
}

// 启动测量
void start_measurement() {
    i2c_write_byte(CMD_STAT_REG, MEASURE_CMD);  // 启动测量
    while (i2c_read_byte(CMD_STAT_REG) != 0x01) {  // 等待命令完成
        sleep_ms(10);
    }
    printf("Measurement started.\n");
}

// 停止测量
void stop_measurement() {
    i2c_write_byte(CMD_STAT_REG, STOP_CMD);  // 停止测量
    while (i2c_read_byte(CMD_STAT_REG) != 0x00) {  // 等待命令完成
        sleep_ms(10);
    }
    printf("Measurement stopped.\n");
}

// 读取测量结果
void read_measurement_results() {
    uint8_t result_id = i2c_read_byte(0x20);  // 读取结果ID
    if (result_id == 0x10) {  // 检查是否为测量结果
        uint8_t data[132];  // 假设结果数据最大为32字节
        uint8_t reg=0x20;
        i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &reg, 1, true);  // 设置读取起始地址
        i2c_read_blocking(I2C_PORT, I2C_ADDRESS, data, sizeof(data), false);  // 读取结果数据
        uint8_t dm;
        uint8_t dl;
        reg=0x3A;
        i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &reg, 1, true);  // 设置读取起始地址
        i2c_read_blocking(I2C_PORT, I2C_ADDRESS, &dm, 1, false);  // 读取结果数据
        
        reg=0x39;
        i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &reg, 1, true);  // 设置读取起始地址
        i2c_read_blocking(I2C_PORT, I2C_ADDRESS, &dl, 1, false);  // 读取结果数据
 
         printf("value: 0x%02X%02X\n", dm, dl);
        printf("\n");
    } else {
        printf("Unexpected result ID: 0x%02X\n", result_id);
    }
}

void gpio_callback(uint gpio, uint32_t events) {
    uint8_t dataa=0;
    printf("measure finish\n");
    dataa=i2c_read_byte(0xe1);
    i2c_write_byte(0xe1,dataa);
    read_measurement_results();
}

int main() {
    stdio_init_all();  // 初始化标准I/O
    i2c_init_bus();    // 初始化I²C总线
    gpio_init(22);
    gpio_set_dir(22, GPIO_OUT);
    gpio_init(21);
    gpio_set_dir(21, GPIO_IN);
    gpio_put(22, 1);
    i2c_write_byte(0xE0,0x01);

    sleep_ms(5000);  //等待串口

    printf("Checking device readiness...\n");
    check_device_ready();  // 检查设备是否准备好通信
    printf("register value: 0x%02X\n", i2c_read_byte(0));


    uint8_t bu[5]={0x08,0x14,0x01,0x29,0xc1};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, bu, 5, false);
    uint8_t re=0x08;
    uint8_t dat[3];
    uint8_t da[3];
    uint8_t i=0;

    while(true)
    {
        sleep_ms(20);
        i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &re, 1, true);  // 发送寄存器地址
        i2c_read_blocking(I2C_PORT, I2C_ADDRESS, dat, 3, false); // 读取数据
        printf("ready register value: 0x%02X 0x%02X 0x%02X\n", dat[0], dat[1], dat[2]);
        if(dat[2]==0xff)
            break;
    }
    set_address(0x0000);

    while(true)
    {
        sleep_ms(20);
        i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &re, 1, true);  // 发送寄存器地址
        i2c_read_blocking(I2C_PORT, I2C_ADDRESS, dat, 3, false); // 读取数据
        printf("address register value: 0x%02X 0x%02X 0x%02X\n", dat[0], dat[1], dat[2]);
        if(dat[2]==0xff)
            break;
    }
    int ii=0;
    while (true)
    {
        
        write_ram((void*)(tmf882x_image+ii), 20);

        while(true)
        {
            sleep_ms(1);
            i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &re, 1, true);  // 发送寄存器地址
            i2c_read_blocking(I2C_PORT, I2C_ADDRESS, dat, 3, false); // 读取数据
            if(dat[2]==0xff)
                break;
            else
                printf("address register value: 0x%02X 0x%02X 0x%02X\n", dat[0], dat[1], dat[2]);

        }
        ii+=20;
        if(ii==2620)
        {
        write_ram((void*)(tmf882x_image+ii), 16);
        sleep_ms(2);
        break;
        }
        printf("%d\n",ii);

    }

    ram_remap_reset();

    // 等待设备重启并检查APPID
    sleep_ms(3);  // 等待2.5秒
    uint8_t appid = i2c_read_byte(APPID_REG);
     printf("APPID: 0x%02X\n", appid);

    load_common_config();
    uint8_t r=0x20;

    while(true)
    {
        i2c_write_blocking(I2C_PORT, I2C_ADDRESS, &r, 1, true);  // 发送寄存器地址
        i2c_read_blocking(I2C_PORT, I2C_ADDRESS, da, 4, false); // 读取数据
        printf("Check configuration register value: 0x%02X 0x%02X 0x%02X 0x%02X\n", da[0], da[1], da[2], da[3]);
        if((da[0]==0x16)&&(da[2]==0xbc)&&(da[3]==0x00))
            break;
        sleep_ms(20);           
    }

    // 步骤2: 配置测量周期为100ms
    set_measurement_period();

    // 步骤3: 选择SPAD掩码6
    set_spad_mask();
    i2c_write_byte(0x31, 0x03);  
    write_common_config();

    printf("Check factory register value: 0x%02X\n", i2c_read_byte(0x07));

    // 步骤5: 启用结果中断
    enable_interrupts();

    // 步骤6: 清除中断
    clear_interrupts();
         
    gpio_set_irq_enabled_with_callback(21, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // 步骤7: 启动测量
    start_measurement();

    while (1) {
        // 主循环
        ;
    }

    return 0;
}