#include "schedule_manager.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include "dirent.h"
#include <sys/stat.h>
#include "ntp_time.h"

static const char *TAG = "SCHEDULE_MGR";

void schedule_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS for schedule storage");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        ESP_ERROR_CHECK(ret);
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        ESP_ERROR_CHECK(ret);
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

    // Create summary file if it doesn't exist
    FILE* f = fopen(SUMMARY_FILE_PATH, "r");
    if (f == NULL) {
        f = fopen(SUMMARY_FILE_PATH, "w");
        if (f != NULL) {
            fputs("[]", f);
            fclose(f);
            ESP_LOGI(TAG, "Created empty summary file");
        }
    } else {
        fclose(f);
    }

    ESP_LOGI(TAG, "Schedule manager initialized");
}

bool save_daily_schedule(const char* date, const daily_schedule_t* schedule)
{
    if (!is_valid_date(date)) {
        ESP_LOGE(TAG, "Invalid date format: %s", date);
        return false;
    }

    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", SCHEDULE_FILE_PATH, date);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "date", date);
    
    cJSON* events_array = cJSON_CreateArray();
    for (int i = 0; i < schedule->event_count; i++) {
        cJSON* event_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(event_obj, "id", schedule->events[i].id);
        cJSON_AddNumberToObject(event_obj, "year", schedule->events[i].year);
        cJSON_AddNumberToObject(event_obj, "month", schedule->events[i].month);
        cJSON_AddNumberToObject(event_obj, "day", schedule->events[i].day);
        cJSON_AddNumberToObject(event_obj, "hour", schedule->events[i].hour);
        cJSON_AddNumberToObject(event_obj, "minute", schedule->events[i].minute);
        cJSON_AddNumberToObject(event_obj, "duration_seconds", schedule->events[i].duration_seconds);
        cJSON_AddBoolToObject(event_obj, "enabled", schedule->events[i].enabled);
        
        cJSON_AddItemToArray(events_array, event_obj);
    }
    cJSON_AddItemToObject(root, "events", events_array);

    char* json_str = cJSON_Print(root);
    FILE* f = fopen(filepath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath);
        cJSON_Delete(root);
        free(json_str);
        return false;
    }

    fputs(json_str, f);
    fclose(f);
    
    cJSON_Delete(root);
    free(json_str);

    // Update summary file
    return update_summary_file();
}

bool load_daily_schedule(const char* date, daily_schedule_t* schedule)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", SCHEDULE_FILE_PATH, date);

    FILE* f = fopen(filepath, "r");
    if (f == NULL) {
        ESP_LOGD(TAG, "Schedule file not found: %s", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* json_str = malloc(fsize + 1);
    fread(json_str, 1, fsize, f);
    json_str[fsize] = 0;
    fclose(f);

    cJSON* root = cJSON_Parse(json_str);
    free(json_str);

    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON for date %s", date);
        return false;
    }

    strncpy(schedule->date, date, sizeof(schedule->date) - 1);
    schedule->date[sizeof(schedule->date) - 1] = '\0';

    cJSON* events_array = cJSON_GetObjectItem(root, "events");
    if (cJSON_IsArray(events_array)) {
        schedule->event_count = 0;
        cJSON* event_item;
        cJSON_ArrayForEach(event_item, events_array) {
            if (schedule->event_count >= MAX_SCHEDULES_PER_DAY) {
                break;
            }

            schedule_event_t* event = &schedule->events[schedule->event_count];
            
            cJSON* id = cJSON_GetObjectItem(event_item, "id");
            cJSON* year = cJSON_GetObjectItem(event_item, "year");
            cJSON* month = cJSON_GetObjectItem(event_item, "month");
            cJSON* day = cJSON_GetObjectItem(event_item, "day");
            cJSON* hour = cJSON_GetObjectItem(event_item, "hour");
            cJSON* minute = cJSON_GetObjectItem(event_item, "minute");
            cJSON* duration = cJSON_GetObjectItem(event_item, "duration_seconds");
            cJSON* enabled = cJSON_GetObjectItem(event_item, "enabled");

            if (cJSON_IsString(id)) {
                strncpy(event->id, id->valuestring, sizeof(event->id) - 1);
            }
            if (cJSON_IsNumber(year)) event->year = year->valueint;
            if (cJSON_IsNumber(month)) event->month = month->valueint;
            if (cJSON_IsNumber(day)) event->day = day->valueint;
            if (cJSON_IsNumber(hour)) event->hour = hour->valueint;
            if (cJSON_IsNumber(minute)) event->minute = minute->valueint;
            if (cJSON_IsNumber(duration)) event->duration_seconds = duration->valueint;
            if (cJSON_IsBool(enabled)) event->enabled = cJSON_IsTrue(enabled);

            schedule->event_count++;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool update_summary_file(void)
{
    struct dirent* entry;
    DIR* dir = opendir(SCHEDULE_FILE_PATH);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open schedules directory");
        return false;
    }

    cJSON* summary_array = cJSON_CreateArray();

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char* ext = strrchr(entry->d_name, '.');
            if (ext != NULL && strcmp(ext, ".json") == 0) {
                // Remove .json extension
                char date[11];
                strncpy(date, entry->d_name, ext - entry->d_name);
                date[ext - entry->d_name] = '\0';
                
                if (is_valid_date(date)) {
                    cJSON_AddItemToArray(summary_array, cJSON_CreateString(date));
                }
            }
        }
    }
    closedir(dir);

    char* json_str = cJSON_Print(summary_array);
    FILE* f = fopen(SUMMARY_FILE_PATH, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to write summary file");
        cJSON_Delete(summary_array);
        free(json_str);
        return false;
    }

    fputs(json_str, f);
    fclose(f);

    cJSON_Delete(summary_array);
    free(json_str);
    return true;
}

bool check_schedule_event(const schedule_event_t* event, struct tm* current_time)
{
    if (!event->enabled) return false;

    return (event->year == current_time->tm_year + 1900 &&
            event->month == current_time->tm_mon + 1 &&
            event->day == current_time->tm_mday &&
            event->hour == current_time->tm_hour &&
            event->minute == current_time->tm_min);
}

daily_schedule_t* get_today_schedule(void)
{
    static daily_schedule_t today_schedule;
    char today[11];
    
    struct tm current_time;
    get_current_time(&current_time);
    date_to_string(&current_time, today, sizeof(today));
    
    if (load_daily_schedule(today, &today_schedule)) {
        return &today_schedule;
    }
    
    return NULL;
}

// Utility functions
bool is_valid_date(const char* date)
{
    if (strlen(date) != 10) return false;
    if (date[4] != '-' || date[7] != '-') return false;
    
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (date[i] < '0' || date[i] > '9') return false;
    }
    
    return true;
}

void date_to_string(struct tm* time, char* buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "%04d-%02d-%02d", 
             time->tm_year + 1900, time->tm_mon + 1, time->tm_mday);
}