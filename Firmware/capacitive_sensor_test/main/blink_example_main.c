#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h" // Thư viện delay micro-giây chính xác để lọc nhiễu 50Hz

#define PAD_D4_CHANNEL TOUCH_PAD_NUM0  // GPIO 4
#define PAD_D15_CHANNEL TOUCH_PAD_NUM3 // GPIO 15
#define LED_PIN GPIO_NUM_2
#define BOOT_BTN_PIN GPIO_NUM_0

// Ngưỡng tụt để báo có thuốc (Sau khi đã ép phẳng sóng nhiễu)
#define D4_DROP_THRESH 2.5f
#define D15_DROP_THRESH 2.0f
#define HAND_DROP_THRESH 20.0f

static float base_d4 = 0;
static float base_d15 = 0;

// Hàm đọc và khử nhiễu điện lưới 50Hz (Đo liên tiếp trong 100ms = 5 chu kỳ điện 50Hz)
void read_clean_touch(float *out_d4, float *out_d15)
{
    uint32_t sum_d4 = 0, sum_d15 = 0;
    uint16_t v4 = 0, v15 = 0;
    const int samples = 100;

    for (int i = 0; i < samples; i++)
    {
        touch_pad_read_filtered(PAD_D4_CHANNEL, &v4);
        touch_pad_read_filtered(PAD_D15_CHANNEL, &v15);
        sum_d4 += v4;
        sum_d15 += v15;
        // Delay đúng 1000 us (1ms) -> 100 mẫu = 100ms (ôm trọn 5 vòng sóng 50Hz)
        esp_rom_delay_us(1000);
    }
    *out_d4 = (float)sum_d4 / samples;
    *out_d15 = (float)sum_d15 / samples;
}

// Hàm lấy mốc chuẩn lúc trống
void calibrate_empty(void)
{
    printf("\n>>> DANG LAY MOC CHUAN TRONG (3 GIAY)... <<<\n");
    float d4, d15;
    float sum4 = 0, sum15 = 0;
    const int loops = 20;

    for (int i = 0; i < loops; i++)
    {
        read_clean_touch(&d4, &d15);
        sum4 += d4;
        sum15 += d15;
        gpio_set_level(LED_PIN, i % 2);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    gpio_set_level(LED_PIN, 0);

    base_d4 = sum4 / loops;
    base_d15 = sum15 / loops;
    printf(">>> MOC CHUAN TRONG -> D4_Base: %.1f | D15_Base: %.1f <<<\n\n", base_d4, base_d15);
}

void app_main(void)
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BOOT_BTN_PIN);
    gpio_set_direction(BOOT_BTN_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_BTN_PIN, GPIO_PULLUP_ONLY);

    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

    // QUAN TRỌNG: Giảm thời gian nghỉ (sleep_cycle) từ 0x1000 xuống 0x0020
    // để triệt tiêu hiện tượng phách tần số gây trôi số hình sin!
    touch_pad_set_meas_time(0x0020, 0x7FFF);

    touch_pad_config(PAD_D4_CHANNEL, 0);
    touch_pad_config(PAD_D15_CHANNEL, 0);
    touch_pad_filter_start(10);
    vTaskDelay(pdMS_TO_TICKS(500));

    calibrate_empty();

    // Biến lọc trung bình trượt mượt (EMA)
    float smooth_d4 = base_d4;
    float smooth_d15 = base_d15;

    while (1)
    {
        if (gpio_get_level(BOOT_BTN_PIN) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            calibrate_empty();
            smooth_d4 = base_d4;
            smooth_d15 = base_d15;
        }

        float cur_d4, cur_d15;
        read_clean_touch(&cur_d4, &cur_d15);

        // Làm mượt tín hiệu (30% giá trị mới + 70% giá trị cũ)
        smooth_d4 = 0.3f * cur_d4 + 0.7f * smooth_d4;
        smooth_d15 = 0.3f * cur_d15 + 0.7f * smooth_d15;

        float drop_d4 = base_d4 - smooth_d4;
        float drop_d15 = base_d15 - smooth_d15;

        // Phân tích D4
        const char *str_d4 = "[TRONG]   ";
        bool pill_d4 = false;
        if (drop_d4 >= HAND_DROP_THRESH)
        {
            str_d4 = "[CHAM TAY]";
        }
        else if (drop_d4 >= D4_DROP_THRESH)
        {
            str_d4 = "[CO THUOC]";
            pill_d4 = true;
        }

        // Phân tích D15
        const char *str_d15 = "[TRONG]   ";
        bool pill_d15 = false;
        if (drop_d15 >= HAND_DROP_THRESH)
        {
            str_d15 = "[CHAM TAY]";
        }
        else if (drop_d15 >= D15_DROP_THRESH)
        {
            str_d15 = "[CO THUOC]";
            pill_d15 = true;
        }

        printf("D4: %5.1f (Sut:%+5.1f) -> %s | D15: %5.1f (Sut:%+5.1f) -> %s\n",
               smooth_d4, drop_d4, str_d4,
               smooth_d15, drop_d15, str_d15);

        gpio_set_level(LED_PIN, (pill_d4 || pill_d15) ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}