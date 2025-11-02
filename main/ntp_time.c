#include "ntp_time.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "NTP";

static EventGroupHandle_t ntp_event_group;
#define NTP_SYNC_BIT BIT0

static void ntp_time_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronized");
    xEventGroupSetBits(ntp_event_group, NTP_SYNC_BIT);
}

void ntp_time_init(void)
{
    ESP_LOGI(TAG, "Initializing NTP");
    
    ntp_event_group = xEventGroupCreate();
    
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.nist.gov");
    sntp_setservername(2, "time.google.com");
    
    sntp_set_time_sync_notification_cb(ntp_time_callback);
    sntp_init();
    
    // Set timezone to UTC (modify as needed)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

bool ntp_time_sync(void)
{
    ESP_LOGI(TAG, "Waiting for NTP time synchronization...");
    
    // Wait for synchronization with timeout (10 seconds)
    EventBits_t bits = xEventGroupWaitBits(ntp_event_group, NTP_SYNC_BIT, pdFALSE, pdTRUE, 10000 / portTICK_PERIOD_MS);
    
    if (bits & NTP_SYNC_BIT) {
        ESP_LOGI(TAG, "NTP synchronization successful");
        return true;
    } else {
        ESP_LOGE(TAG, "NTP synchronization timeout");
        return false;
    }
}

void get_current_time(struct tm* timeinfo)
{
    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);
}

bool set_time_zone(const char* timezone)
{
    setenv("TZ", timezone, 1);
    tzset();
    return true;
}

bool is_time_synced(void)
{
    return (xEventGroupGetBits(ntp_event_group) & NTP_SYNC_BIT) != 0;
}

void ntp_time_task(void* pvParameters)
{
    ntp_time_init();
    ntp_time_sync();
    
    // Periodic time sync every hour
    while (1) {
        vTaskDelay(3600000 / portTICK_PERIOD_MS); // 1 hour
        if (!is_time_synced()) {
            ESP_LOGW(TAG, "Time not synchronized, re-syncing...");
            ntp_time_sync();
        }
    }
    
    vTaskDelete(NULL);
}