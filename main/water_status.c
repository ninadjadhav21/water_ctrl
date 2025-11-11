#include "water_status.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "WATER_STATUS";

static volatile uint16_t s_level = 0;
static volatile uint8_t  s_empty = 1;

static SemaphoreHandle_t s_mutex = NULL;

#define UART_NUM            UART_NUM_1
#define UART_BAUD_RATE      9600
#define UART_RX_BUF_SIZE    256

// GPIO pins per requirement
#define UART_TX_PIN         GPIO_NUM_1
#define UART_RX_PIN         GPIO_NUM_0
#define WATER_EMPTY_GPIO    GPIO_NUM_3

static void water_status_task(void *arg)
{
    uint8_t cmd = 0xA0;
    uint8_t data[3];

    for (;;) {
        // Send command
        int tx_bytes = uart_write_bytes(UART_NUM, (const char*)&cmd, 1);
        (void)tx_bytes;

        // Small delay to allow sensor to respond
        vTaskDelay(pdMS_TO_TICKS(100));

        // Read up to 3 bytes, wait up to 500ms
        int len = uart_read_bytes(UART_NUM, data, sizeof(data), pdMS_TO_TICKS(500));
        if (len >= 3) {
            uint32_t level_temp = ((data[0] << 16) + (data[1] << 8) + data[2]);
            level_temp *= 100;
            level_temp /= 1000000;
            // sanitize level_temp
            // 5555 is absurdly high value which will never occur in normal operation
            if(level_temp > 1000) level_temp=5555;
            uint16_t new_level = (uint16_t)level_temp;
            uint8_t  new_empty = (uint8_t)gpio_get_level(WATER_EMPTY_GPIO) ? 1 : 0;

            if (s_mutex) {
                if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    s_level = new_level;
                    s_empty = new_empty;
                    xSemaphoreGive(s_mutex);
                }
            } else {
                // fallback without mutex
                s_level = new_level;
                s_empty = new_empty;
            }
            ESP_LOGD(TAG, "Sensor read len=%d level=%u empty=%u", len, new_level, new_empty);
        } else {
            ESP_LOGW(TAG, "Sensor read failed or incomplete (len=%d)", len);
        }

        // Sleep 5 seconds between updates
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void water_status_init(void)
{
    // Create mutex
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
        }
    }

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };
    uart_param_config(UART_NUM, &uart_config);

    // Set pins (TX, RX, RTS, CTS)
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Install UART driver (RX buffer only)
    uart_driver_install(UART_NUM, UART_RX_BUF_SIZE, 0, 0, NULL, 0);

    // Configure water-empty GPIO as input
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << WATER_EMPTY_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf);

    // Create the periodic task
    BaseType_t r = xTaskCreate(water_status_task, "water_status_task", 4096, NULL, tskIDLE_PRIORITY + 3, NULL);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create water_status_task");
    } else {
        ESP_LOGI(TAG, "water_status task started");
    }
}

bool get_water_level_status(uint16_t *level, uint8_t *empty)
{
    if (level == NULL || empty == NULL) return false;

    if (s_mutex) {
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            *level = s_level;
            *empty = s_empty;
            xSemaphoreGive(s_mutex);
            return true;
        } else {
            return false;
        }
    } else {
        // No mutex available (shouldn't happen) — best effort
        *level = s_level;
        *empty = s_empty;
        return true;
    }
}
