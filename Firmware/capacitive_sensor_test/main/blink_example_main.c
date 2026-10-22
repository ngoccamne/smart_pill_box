#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Định nghĩa chân cảm biến hồng ngoại TCRT5000
#define IR_SENSOR_PIN GPIO_NUM_32

void setup_gpio()
{
    // Cấu hình Input cho cảm biến IR
    gpio_config_t ir_conf = {
        .pin_bit_mask = (1ULL << IR_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0, // Module TCRT5000 (có IC LM393) thường đã có sẵn trở kéo lên
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&ir_conf);
}

void app_main(void)
{
    setup_gpio();

    printf("Bat dau test cam bien IR TCRT5000...\n");

    while (1)
    {
        // Đọc giá trị số từ chân DO của cảm biến
        int ir_state = gpio_get_level(IR_SENSOR_PIN);

        // Đa số module TCRT5000: Có vật cản (phản xạ hồng ngoại) -> LOW (0)
        // Không có vật cản -> HIGH (1)
        if (ir_state == 0)
        {
            printf("Trang thai IR: 0 -> Co vien thuoc!\n");
        }
        else
        {
            printf("Trang thai IR: 1 -> Ngan trong\n");
        }

        // Chờ 500ms để dòng log trên Terminal không bị trôi quá nhanh
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}