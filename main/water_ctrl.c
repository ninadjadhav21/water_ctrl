#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "water_ctrl.h"
#include "schedule_manager.h"
#include "ntp_time.h"
#include "string.h"

static const char *TAG = "WATERING";

// Don't use GPIO2 as it is shared with BOOT, it will cause problem because of strapping
#define PUMP_GPIO      GPIO_NUM_4
#define WATERING_QUEUE_SIZE 10

static QueueHandle_t watering_queue = NULL;
static TaskHandle_t watering_task_handle = NULL;
static daily_schedule_t current_daily_schedule;
static bool schedule_loaded = false;
static char current_loaded_date[11] = {0};

void watering_controller_init(void)
{
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PUMP_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Initialize schedule manager
    schedule_manager_init();

    // Create watering queue
    watering_queue = xQueueCreate(WATERING_QUEUE_SIZE, sizeof(watering_command_t));

    // Start NTP time synchronization task
    xTaskCreate(ntp_time_task, "ntp_time_task", 4096, NULL, 5, NULL);

    // Start watering task
    xTaskCreate(watering_task, "watering_task", 4096, NULL, 5, &watering_task_handle);

    ESP_LOGI(TAG, "Watering controller initialized");
}

void start_manual_watering(int duration_seconds)
{
    watering_command_t cmd = {
        .type = MANUAL_WATERING,
        .duration = duration_seconds
    };
    xQueueSend(watering_queue, &cmd, portMAX_DELAY);
}

void add_scheduled_watering(const schedule_event_t* event)
{
    watering_command_t cmd = {
        .type = SCHEDULED_WATERING,
        .duration = event->duration_seconds,
        .event_id = {0}
    };
    strncpy(cmd.event_id, event->id, sizeof(cmd.event_id) - 1);
    xQueueSend(watering_queue, &cmd, portMAX_DELAY);
}

static void water_plants(int duration_seconds, const char* event_id)
{
    ESP_LOGI(TAG, "Watering plants for %d seconds (Event: %s)", 
             duration_seconds, event_id ? event_id : "manual");
    
    gpio_set_level(PUMP_GPIO, 1);
    vTaskDelay(duration_seconds * 1000 / portTICK_PERIOD_MS);
    gpio_set_level(PUMP_GPIO, 0);
    
    ESP_LOGI(TAG, "Watering completed");
}

static void load_todays_schedule(void)
{
    struct tm current_time;
    get_current_time(&current_time);
    
    char today[11];
    date_to_string(&current_time, today, sizeof(today));
    
    // Only reload if date changed or not loaded
    if (strcmp(current_loaded_date, today) != 0 || !schedule_loaded) {
        if (load_daily_schedule(today, &current_daily_schedule)) {
            strncpy(current_loaded_date, today, sizeof(current_loaded_date) - 1);
            schedule_loaded = true;
            ESP_LOGI(TAG, "Loaded schedule for %s with %d events", 
                     today, current_daily_schedule.event_count);
        } else {
            schedule_loaded = false;
            ESP_LOGD(TAG, "No schedule found for %s", today);
        }
    }
}

static void check_scheduled_events(void)
{
    if (!schedule_loaded) return;
    
    struct tm current_time;
    get_current_time(&current_time);
    
    for (int i = 0; i < current_daily_schedule.event_count; i++) {
        schedule_event_t* event = &current_daily_schedule.events[i];
        
        if (check_schedule_event(event, &current_time)) {
            ESP_LOGI(TAG, "Triggering scheduled watering: %s", event->id);
            add_scheduled_watering(event);
            
            // Optional: Mark event as executed to prevent re-triggering
            // This would require modifying the schedule file
        }
    }
}

void watering_task(void *pvParameters)
{
    watering_command_t cmd;
    
    ESP_LOGI(TAG, "Watering task started");
    
    while (1) {
        // Check for manual watering commands (non-blocking)
        if (xQueueReceive(watering_queue, &cmd, 1000 / portTICK_PERIOD_MS)) {
            water_plants(cmd.duration, 
                        cmd.type == SCHEDULED_WATERING ? cmd.event_id : "manual");
        }
        
        // Only check schedules if time is synchronized
        if (is_time_synced()) {
            // Reload today's schedule (if needed)
            load_todays_schedule();
            
            // Check for scheduled events
            check_scheduled_events();
        } else {
            ESP_LOGW(TAG, "Time not synchronized, skipping schedule check");
        }
    }
    
    vTaskDelete(NULL);
}

// Public API for schedule management
bool watering_add_schedule(const char* date, const schedule_event_t* event)
{
    daily_schedule_t schedule;
    
    // Try to load existing schedule
    if (!load_daily_schedule(date, &schedule)) {
        // Create new schedule if it doesn't exist
        strncpy(schedule.date, date, sizeof(schedule.date) - 1);
        schedule.event_count = 0;
    }
    
    // Check if we have space for more events
    if (schedule.event_count >= MAX_SCHEDULES_PER_DAY) {
        ESP_LOGE(TAG, "Maximum events per day reached");
        return false;
    }
    
    // Add the event
    schedule.events[schedule.event_count] = *event;
    schedule.event_count++;
    
    // Save the schedule
    return save_daily_schedule(date, &schedule);
}

bool watering_remove_schedule(const char* date, const char* event_id)
{
    daily_schedule_t schedule;
    
    if (!load_daily_schedule(date, &schedule)) {
        return false;
    }
    
    // Find and remove the event
    for (int i = 0; i < schedule.event_count; i++) {
        if (strcmp(schedule.events[i].id, event_id) == 0) {
            // Shift remaining events
            for (int j = i; j < schedule.event_count - 1; j++) {
                schedule.events[j] = schedule.events[j + 1];
            }
            schedule.event_count--;
            
            return save_daily_schedule(date, &schedule);
        }
    }
    
    ESP_LOGE(TAG, "Event %s not found in schedule %s", event_id, date);
    return false;
}