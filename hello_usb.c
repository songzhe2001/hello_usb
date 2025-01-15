#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "tmf882x_image.h"
#include "tmf8821.h"

extern const unsigned char tmf882x_image[];

void gpio_callback(uint gpio, uint32_t events)
{
    uint8_t dataa = 0;
    printf("measure finish\n");
    // dataa = i2c_read_byte(0xe1);
    i2c_write_byte(0xe1, i2c_read_byte(0xe1));
    read_measurement_results();
}

int main()
{
    stdio_init_all(); // 初始化标准I/O
    i2c_init_bus();   // 初始化I²C总线
    gpio_init(22);
    gpio_set_dir(22, GPIO_OUT);
    gpio_init(21);
    gpio_set_dir(21, GPIO_IN);
    gpio_put(22, 1);
    i2c_write_byte(0xE0, 0x01);

    sleep_ms(5000); // 等待串口

    printf("Checking device readiness...\n");
    check_device_ready(); // 检查设备是否准备好通信
    printf("register value: 0x%02X\n", i2c_read_byte(0));

    uint8_t bu[5] = {0x08, 0x14, 0x01, 0x29, 0xc1};
    i2c_write_blocking(I2C_PORT, I2C_ADDRESS, bu, 5, false);
    uint8_t re = 0x08;
    uint8_t dat[3];
    uint8_t da[3];
    uint8_t i = 0;

    check_cmd();
    set_address(0x0000);

    check_cmd();

    int ii = 0;
    while (true)
    {

        write_ram((void *)(tmf882x_image + ii), 20);
        check_cmd();
        ii += 20;
        if (ii == 2620)
        {
            write_ram((void *)(tmf882x_image + ii), 16);
            sleep_ms(2);
            break;
        }
        printf("%d\n", ii);
    }

    ram_remap_reset();

    // 等待设备重启并检查APPID
    sleep_ms(3); // 等待2.5秒
    uint8_t appid = i2c_read_byte(APPID_REG);
    printf("APPID: 0x%02X\n", appid);

    load_common_config();
    uint8_t r = 0x20;

    check_conf();

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

    while (1)
    {
        // 主循环
        ;
    }

    return 0;
}