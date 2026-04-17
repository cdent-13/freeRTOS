/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "sdkconfig.h"
#include "esp_adc/adc_cali.h"
#include "soc/soc_caps.h"


static const char *TAG = "ESP32-S3";
static adc_channel_t channel_arr[1] = {1};

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_STRIP

static led_strip_handle_t led_strip;

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    } else {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
#error "unsupported LED type"
#endif

#if CONFIG_ADC_MODE_ONESHOT
    void configure_oneshot_adc(adc_unit_t unit_num, adc_atten_t atten_lvl, int gpio_pin, adc_oneshot_unit_handle_t *out_handle){
        ESP_LOGI("SETUP", "Configuring ADC unit and channel");

        ESP_LOGI("SETUP", "Configuring for oneshot mode");

        adc_oneshot_unit_handle_t adc_unit_handle;
        adc_oneshot_unit_init_cfg_t adc_unit_cfg = {
            .unit_id = unit_num,
            .clk_src = 0,
            .ulp_mode = false,
        };

        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_unit_cfg, &adc_unit_handle));

        adc_oneshot_chan_cfg_t adc_chan_cfg = {
            .atten = atten_lvl,
            .bitwidth = ADC_BITWIDTH_DEFAULT
        };
        adc_unit_t unit; 
        adc_channel_t channel;
        adc_oneshot_io_to_channel(gpio_pin, &unit, &channel);

        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit_handle, channel, &adc_chan_cfg));

        *out_handle = adc_unit_handle;
    }
    
#elif CONFIG_ADC_MODE_CONTINUOUS
    void configure_continuous_adc(adc_unit_t unit_num, adc_atten_t atten_lvl, adc_continuous_handle_t *out_handle){
        ESP_LOGI("SETUP", "Configuring ADC unit and channel");

        ESP_LOGI("SETUP", "Configuring for continuous mode");

        adc_continuous_handle_t adc_con_handle;
        adc_continuous_handle_cfg_t adc_con_handle_cfg = {
            .conv_frame_size = 256,
            .max_store_buf_size = 1024
        };
        adc_continuous_new_handle(&adc_con_handle_cfg, &adc_con_handle);

        adc_continuous_config_t adc_digi_cfg = {
            .sample_freq_hz = 20 * 1000,
            .conv_mode = ADC_CONV_SINGLE_UNIT_1
        };

        adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
        adc_channel_t channel;

        adc_continuous_io_to_channel(1, unit_num, &channel);

        adc_digi_cfg.pattern_num = channel;

        for (int i = 0; i < channel; i++){
            adc_pattern[i].atten = atten_lvl,
            adc_pattern[i].channel = channel_arr[i] & 0x7;
            adc_pattern[i].unit = unit_num;
            adc_pattern[i].bit_width = ADC_BITWIDTH_DEFAULT;

            ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
            ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
            ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
        }
        adc_digi_cfg.adc_pattern = adc_pattern;
        
        ESP_ERROR_CHECK(adc_continuous_config(adc_con_handle, &adc_digi_cfg));

        *out_handle = adc_con_handle;
    }
#endif

#if CONFIG_ADC_MODE_ONESHOT
void configure_cali_handle(adc_unit_t unit_num, adc_atten_t atten_lvl, adc_cali_handle_t *out_handle){
    adc_cali_handle_t cali_handle;
    adc_cali_curve_fitting_config_t cali_curv_fit_config = {
        .unit_id = unit_num,
        .atten = atten_lvl,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_curv_fit_config, &cali_handle));

    *out_handle = cali_handle;
}
#endif

void app_main(void)
{
    configure_led();

#if CONFIG_ADC_MODE_ONESHOT
    adc_oneshot_unit_handle_t adc_handle;        
    configure_oneshot_adc(ADC_UNIT_1, ADC_ATTEN_DB_12, 1, &adc_handle);
    
    adc_cali_handle_t cali_handle;
    configure_cali_handle(ADC_UNIT_1, ADC_ATTEN_DB_12, &cali_handle);

#elif CONFIG_ADC_MODE_CONTINUOUS
    configure_continuous_adc();
#endif

    while (1) {

        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);

        int d_raw;
        int voltage;
        
        adc_oneshot_read(adc_handle, 1, &d_raw);
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, d_raw, &voltage));

        ESP_LOGI(TAG, "Voltage: %d", voltage);
    }
}
