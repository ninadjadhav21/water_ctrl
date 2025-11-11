//////////////// Enhance
#include <esp_http_server.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include "water_ctrl.h"
#include "schedule_manager.h"
#include "ntp_time.h"
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>            // <--- added
#include "water_status.h"      // <--- added

static const char *TAG = "WEB_SERVER";

static httpd_handle_t server = NULL;

// HTML content for the main page
const char* error_html = 
            "<!DOCTYPE html>"
            "<html>"
            "<head><title>Error</title></head>"
            "<body>"
            "<h1>Error Loading Page</h1>"
            "<p>Failed to load index.html from storage</p>"
            "</body>"
            "</html>";

// Helper function declarations
static esp_err_t serve_error_html(httpd_req_t *req);
static esp_err_t serve_large_file_chunked(httpd_req_t *req);

// Handler for serving the main page from SPIFFS with fallback
static esp_err_t root_get_handler(httpd_req_t *req)
{
    // Try to open the file from SPIFFS
    FILE* file = fopen("/spiffs/index.html", "r");
    if (file == NULL) {
        ESP_LOGW(TAG, "index.html not found in SPIFFS, using embedded version");
        // this will return error
        return serve_error_html(req);
    }

    // Get file size
    if (fseek(file, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "Failed to seek in file");
        fclose(file);
        return serve_error_html(req);
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        ESP_LOGE(TAG, "Failed to get file size");
        fclose(file);
        return serve_error_html(req);
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "Failed to rewind file");
        fclose(file);
        return serve_error_html(req);
    }
    // For larger files, use chunked transfer
    return serve_large_file_chunked(req);
}

// Helper function to serve embedded HTML as fallback
static esp_err_t serve_error_html(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    ESP_LOGI(TAG, "Serving error HTML version");
    return httpd_resp_send(req, error_html, strlen(error_html));
}

// Helper function to serve large files in chunks
static esp_err_t serve_large_file_chunked(httpd_req_t *req)
{
    FILE* file = fopen("/spiffs/index.html", "r");
    if (file == NULL) {
        return serve_error_html(req);
    }

    httpd_resp_set_type(req, "text/html");

    char chunk[1024];
    size_t total_sent = 0;
    size_t bytes_read;

    while ((bytes_read = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, bytes_read) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send chunk at offset %zu", total_sent);
            fclose(file);
            return ESP_FAIL;
        }
        total_sent += bytes_read;
    }

    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);

    ESP_LOGI(TAG, "Served large index.html in chunks (%zu bytes)", total_sent);
    return ESP_OK;
}


// Handler for getting system status
static esp_err_t status_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Get current time
    struct tm current_time;
    get_current_time(&current_time);
    
    cJSON_AddStringToObject(root, "status", "active");
    cJSON_AddBoolToObject(root, "time_synced", is_time_synced());
    cJSON_AddStringToObject(root, "current_time", asctime(&current_time));
    cJSON_AddNumberToObject(root, "timestamp", time(NULL));
    
    const char *response = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    cJSON_Delete(root);
    free((void*)response);
    return ESP_OK;
}

// Handler for manual watering
static esp_err_t water_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"Water post received\n");
    char buf[100];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (root != NULL) {
        cJSON *duration_item = cJSON_GetObjectItem(root, "duration");
        if (cJSON_IsNumber(duration_item)) {
            int duration = duration_item->valueint;
            if (duration > 0 && duration <= 3600) {
                start_manual_watering(duration);
                ESP_LOGI(TAG, "Manual watering started for %d seconds", duration);
                httpd_resp_send(req, "OK", 2);
            } else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Duration must be 1-3600 seconds");
            }
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid duration");
        }
        cJSON_Delete(root);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }
    
    return ESP_OK;
}

// Handler for getting schedule for specific date
static esp_err_t schedule_get_handler(httpd_req_t *req)
{
    char date[11] = {0};
    
    // Get date from query parameter
    char query_str[50];
    if (httpd_req_get_url_query_len(req) > 0) {
        if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
            char param[20];
            if (httpd_query_key_value(query_str, "date", param, sizeof(param)) == ESP_OK) {
                strncpy(date, param, sizeof(date) - 1);
            }
        }
    }
    
    if (strlen(date) == 0) {
        // If no date provided, use today's date
        struct tm current_time;
        get_current_time(&current_time);
        if (strftime(date, sizeof(date), "%Y-%m-%d", &current_time) == 0) {
            ESP_LOGE(TAG, "Failed to format current date");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }
    
    if (!is_valid_date(date)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid date format. Use YYYY-MM-DD");
        return ESP_FAIL;
    }
    
    daily_schedule_t schedule;
    if (load_daily_schedule(date, &schedule)) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "date", schedule.date);
        
        cJSON *events_array = cJSON_CreateArray();
        for (int i = 0; i < schedule.event_count; i++) {
            cJSON *event_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(event_obj, "id", schedule.events[i].id);
            cJSON_AddNumberToObject(event_obj, "year", schedule.events[i].year);
            cJSON_AddNumberToObject(event_obj, "month", schedule.events[i].month);
            cJSON_AddNumberToObject(event_obj, "day", schedule.events[i].day);
            cJSON_AddNumberToObject(event_obj, "hour", schedule.events[i].hour);
            cJSON_AddNumberToObject(event_obj, "minute", schedule.events[i].minute);
            cJSON_AddNumberToObject(event_obj, "duration_seconds", schedule.events[i].duration_seconds);
            cJSON_AddBoolToObject(event_obj, "enabled", schedule.events[i].enabled);
            
            cJSON_AddItemToArray(events_array, event_obj);
        }
        cJSON_AddItemToObject(root, "events", events_array);
        
        char *response = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        
        cJSON_Delete(root);
        free(response);
    } else {
        // Return empty schedule if no file exists
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "date", date);
        cJSON *events_array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "events", events_array);
        
        char *response = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        
        cJSON_Delete(root);
        free(response);
    }
    
    return ESP_OK;
}

// Handler for creating/updating schedule
static esp_err_t schedule_post_handler(httpd_req_t *req)
{
    char date[11] = {0};
    
    // Get date from query parameter
    char query_str[50];
    if (httpd_req_get_url_query_len(req) > 0) {
        if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
            char param[20];
            if (httpd_query_key_value(query_str, "date", param, sizeof(param)) == ESP_OK) {
                strncpy(date, param, sizeof(date) - 1);
            }
        }
    }
    
    if (!is_valid_date(date)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid date format. Use YYYY-MM-DD");
        return ESP_FAIL;
    }
    
    // Read JSON data
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    daily_schedule_t schedule;
    strncpy(schedule.date, date, sizeof(schedule.date) - 1);
    schedule.event_count = 0;
    
    cJSON *events_array = cJSON_GetObjectItem(root, "events");
    if (cJSON_IsArray(events_array)) {
        cJSON *event_item;
        cJSON_ArrayForEach(event_item, events_array) {
            if (schedule.event_count >= MAX_SCHEDULES_PER_DAY) {
                ESP_LOGW(TAG, "Maximum events per day reached for date %s", date);
                break;
            }
            
            schedule_event_t event = {0};
            
            cJSON *id = cJSON_GetObjectItem(event_item, "id");
            cJSON *year = cJSON_GetObjectItem(event_item, "year");
            cJSON *month = cJSON_GetObjectItem(event_item, "month");
            cJSON *day = cJSON_GetObjectItem(event_item, "day");
            cJSON *hour = cJSON_GetObjectItem(event_item, "hour");
            cJSON *minute = cJSON_GetObjectItem(event_item, "minute");
            cJSON *duration = cJSON_GetObjectItem(event_item, "duration_seconds");
            cJSON *enabled = cJSON_GetObjectItem(event_item, "enabled");
            
            if (cJSON_IsString(id)) {
                strncpy(event.id, id->valuestring, sizeof(event.id) - 1);
            } else {
                // Generate unique ID if not provided
                snprintf(event.id, sizeof(event.id), "event_%d_%d", 
                         schedule.event_count, (int)time(NULL));
            }
            
            if (cJSON_IsNumber(year)) event.year = year->valueint;
            if (cJSON_IsNumber(month)) event.month = month->valueint;
            if (cJSON_IsNumber(day)) event.day = day->valueint;
            if (cJSON_IsNumber(hour)) event.hour = hour->valueint;
            if (cJSON_IsNumber(minute)) event.minute = minute->valueint;
            if (cJSON_IsNumber(duration)) {
                event.duration_seconds = duration->valueint;
                // Validate duration
                if (event.duration_seconds < 1 || event.duration_seconds > 3600) {
                    event.duration_seconds = 30; // Default to 30 seconds
                }
            } else {
                event.duration_seconds = 30; // Default duration
            }
            
            if (cJSON_IsBool(enabled)) {
                event.enabled = cJSON_IsTrue(enabled);
            } else {
                event.enabled = true;
            }
            
            // Validate time
            if (event.hour < 0 || event.hour > 23) event.hour = 0;
            if (event.minute < 0 || event.minute > 59) event.minute = 0;
            
            schedule.events[schedule.event_count++] = event;
            ESP_LOGI(TAG, "Added event %s for %s at %02d:%02d", 
                     event.id, date, event.hour, event.minute);
        }
    }
    
    cJSON_Delete(root);
    
    if (save_daily_schedule(date, &schedule)) {
        ESP_LOGI(TAG, "Schedule saved for %s with %d events", date, schedule.event_count);
        httpd_resp_send(req, "OK", 2);
    } else {
        ESP_LOGE(TAG, "Failed to save schedule for %s", date);
        httpd_resp_send_500(req);
    }
    
    return ESP_OK;
}

// Handler for deleting a schedule
static esp_err_t schedule_delete_handler(httpd_req_t *req)
{
    char date[11] = {0};
    
    // Get date from query parameter
    char query_str[50];
    if (httpd_req_get_url_query_len(req) > 0) {
        if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
            char param[20];
            if (httpd_query_key_value(query_str, "date", param, sizeof(param)) == ESP_OK) {
                strncpy(date, param, sizeof(date) - 1);
            }
        }
    }
    
    if (!is_valid_date(date)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid date format");
        return ESP_FAIL;
    }
    
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", SCHEDULE_FILE_PATH, date);
    
    if (remove(filepath) == 0) {
        update_summary_file();
        ESP_LOGI(TAG, "Schedule deleted for %s", date);
        httpd_resp_send(req, "OK", 2);
    } else {
        ESP_LOGE(TAG, "Failed to delete schedule for %s", date);
        httpd_resp_send_404(req);
    }
    
    return ESP_OK;
}

static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

// New handler for water level status (calls get_water_level_status)
static esp_err_t water_status_get_handler(httpd_req_t *req)
{
    uint16_t level = 0;
    uint8_t empty = 0;

    // Call to hardware/control layer to populate level and empty.
    // Assume function is declared in water_ctrl.h:
    //    bool get_water_level_status(uint16_t *level, uint8_t *empty);
    // We call it and ignore boolean return — still return current values.
    get_water_level_status(&level, &empty);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddNumberToObject(root, "level", level);
    cJSON_AddBoolToObject(root, "empty", empty ? cJSON_True : cJSON_False);

    char *response = cJSON_Print(root);
    if (!response) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    cJSON_Delete(root);
    free(response);
    return ESP_OK;
}

// URI handler structures
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t status_get = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = status_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t water_post = {
    .uri       = "/api/water",
    .method    = HTTP_POST,
    .handler   = water_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t schedule_get = {
    .uri       = "/api/schedule",
    .method    = HTTP_GET,
    .handler   = schedule_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t schedule_post = {
    .uri       = "/api/schedule",
    .method    = HTTP_POST,
    .handler   = schedule_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t schedule_delete = {
    .uri       = "/api/schedule",
    .method    = HTTP_DELETE,
    .handler   = schedule_delete_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t favicon_icon = {
    .uri       = "/favicon.ico",  // Match all URIs of type /path/to/file
    .method    = HTTP_GET,
    .handler   = favicon_get_handler,
    .user_ctx  = NULL    // Pass server data as context
};

// New URI for water status
static const httpd_uri_t water_status_get = {
    .uri       = "/api/water_status",
    .method    = HTTP_GET,
    .handler   = water_status_get_handler,
    .user_ctx  = NULL
};




void start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    
    ESP_LOGI(TAG, "Starting web server on port: %d", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &status_get);
        httpd_register_uri_handler(server, &water_post);
        httpd_register_uri_handler(server, &schedule_get);
        httpd_register_uri_handler(server, &schedule_post);
        httpd_register_uri_handler(server, &schedule_delete);
        httpd_register_uri_handler(server, &favicon_icon);

        // Register new water status handler
        httpd_register_uri_handler(server, &water_status_get);
        
        ESP_LOGI(TAG, "Web server started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}

void stop_web_server(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}