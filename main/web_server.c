// #include <esp_http_server.h>
// #include "esp_log.h"
// #include "cJSON.h"
// #include "watering_controller.h"

// static const char *TAG = "WEB_SERVER";

// static httpd_handle_t server = NULL;

// /* Handler for getting system status */
// static esp_err_t status_get_handler(httpd_req_t *req)
// {
//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "status", "active");
//     cJSON_AddBoolToObject(root, "auto_mode", true);
    
//     const char *response = cJSON_Print(root);
//     httpd_resp_set_type(req, "application/json");
//     httpd_resp_send(req, response, strlen(response));
    
//     cJSON_Delete(root);
//     free((void*)response);
//     return ESP_OK;
// }

// /* Handler for setting watering schedule */
// static esp_err_t schedule_post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    
//     if (ret <= 0) {
//         return ESP_FAIL;
//     }
//     buf[ret] = '\0';
    
//     cJSON *root = cJSON_Parse(buf);
//     if (root != NULL) {
//         watering_schedule_t schedule;
//         schedule.start_hour = cJSON_GetObjectItem(root, "start_hour")->valueint;
//         schedule.start_minute = cJSON_GetObjectItem(root, "start_minute")->valueint;
//         schedule.duration_seconds = cJSON_GetObjectItem(root, "duration")->valueint;
//         schedule.interval_hours = cJSON_GetObjectItem(root, "interval")->valueint;
        
//         set_watering_schedule(schedule);
        
//         cJSON_Delete(root);
//     }
    
//     httpd_resp_send(req, "OK", 2);
//     return ESP_OK;
// }

// /* Handler for manual watering */
// static esp_err_t water_post_handler(httpd_req_t *req)
// {
//     char buf[50];
//     int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    
//     if (ret <= 0) {
//         return ESP_FAIL;
//     }
//     buf[ret] = '\0';
    
//     cJSON *root = cJSON_Parse(buf);
//     if (root != NULL) {
//         int duration = cJSON_GetObjectItem(root, "duration")->valueint;
//         start_manual_watering(duration);
//         cJSON_Delete(root);
//     }
    
//     httpd_resp_send(req, "OK", 2);
//     return ESP_OK;
// }

// /* URI handler structure */
// static const httpd_uri_t status_get = {
//     .uri       = "/api/status",
//     .method    = HTTP_GET,
//     .handler   = status_get_handler,
//     .user_ctx  = NULL
// };

// static const httpd_uri_t schedule_post = {
//     .uri       = "/api/schedule",
//     .method    = HTTP_POST,
//     .handler   = schedule_post_handler,
//     .user_ctx  = NULL
// };

// static const httpd_uri_t water_post = {
//     .uri       = "/api/water",
//     .method    = HTTP_POST,
//     .handler   = water_post_handler,
//     .user_ctx  = NULL
// };


// /* Get all available schedule dates */
// static esp_err_t schedules_dates_get_handler(httpd_req_t *req)
// {
//     char** dates;
//     int count;
    
//     // This function would need to be implemented in schedule_manager.c
//     // to read from the summary file
//     if (get_available_dates(&dates, &count)) {
//         cJSON* root = cJSON_CreateArray();
//         for (int i = 0; i < count; i++) {
//             cJSON_AddItemToArray(root, cJSON_CreateString(dates[i]));
//             free(dates[i]);
//         }
//         free(dates);
        
//         const char* response = cJSON_Print(root);
//         httpd_resp_set_type(req, "application/json");
//         httpd_resp_send(req, response, strlen(response));
        
//         cJSON_Delete(root);
//         free((void*)response);
//     } else {
//         httpd_resp_send_500(req);
//     }
    
//     return ESP_OK;
// }

// /* Get schedule for specific date */
// static esp_err_t schedule_date_get_handler(httpd_req_t *req)
// {
//     char date[11];
//     if (httpd_req_get_url_query_len(req) > 0) {
//         char query[50];
//         httpd_req_get_url_query_str(req, query, sizeof(query));
        
//         char param[10];
//         if (httpd_query_key_value(query, "date", param, sizeof(param)) == ESP_OK) {
//             strncpy(date, param, sizeof(date) - 1);
//             date[sizeof(date) - 1] = '\0';
//         }
//     }
    
//     if (!is_valid_date(date)) {
//         httpd_resp_send_404(req);
//         return ESP_FAIL;
//     }
    
//     daily_schedule_t schedule;
//     if (load_daily_schedule(date, &schedule)) {
//         cJSON* root = cJSON_CreateObject();
//         cJSON_AddStringToObject(root, "date", schedule.date);
        
//         cJSON* events_array = cJSON_CreateArray();
//         for (int i = 0; i < schedule.event_count; i++) {
//             cJSON* event_obj = cJSON_CreateObject();
//             cJSON_AddStringToObject(event_obj, "id", schedule.events[i].id);
//             cJSON_AddNumberToObject(event_obj, "year", schedule.events[i].year);
//             cJSON_AddNumberToObject(event_obj, "month", schedule.events[i].month);
//             cJSON_AddNumberToObject(event_obj, "day", schedule.events[i].day);
//             cJSON_AddNumberToObject(event_obj, "hour", schedule.events[i].hour);
//             cJSON_AddNumberToObject(event_obj, "minute", schedule.events[i].minute);
//             cJSON_AddNumberToObject(event_obj, "duration_seconds", schedule.events[i].duration_seconds);
//             cJSON_AddBoolToObject(event_obj, "enabled", schedule.events[i].enabled);
            
//             cJSON_AddItemToArray(events_array, event_obj);
//         }
//         cJSON_AddItemToObject(root, "events", events_array);
        
//         const char* response = cJSON_Print(root);
//         httpd_resp_set_type(req, "application/json");
//         httpd_resp_send(req, response, strlen(response));
        
//         cJSON_Delete(root);
//         free((void*)response);
//     } else {
//         httpd_resp_send_404(req);
//     }
    
//     return ESP_OK;
// }

// /* Add/Update schedule for specific date */
// static esp_err_t schedule_date_post_handler(httpd_req_t *req)
// {
//     char date[11];
//     if (httpd_req_get_url_query_len(req) > 0) {
//         char query[50];
//         httpd_req_get_url_query_str(req, query, sizeof(query));
        
//         char param[10];
//         if (httpd_query_key_value(query, "date", param, sizeof(param)) == ESP_OK) {
//             strncpy(date, param, sizeof(date) - 1);
//             date[sizeof(date) - 1] = '\0';
//         }
//     }
    
//     if (!is_valid_date(date)) {
//         httpd_resp_send_400(req, "Invalid date format");
//         return ESP_FAIL;
//     }
    
//     char buf[512];
//     int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
//     if (ret <= 0) {
//         return ESP_FAIL;
//     }
//     buf[ret] = '\0';
    
//     cJSON* root = cJSON_Parse(buf);
//     if (root == NULL) {
//         httpd_resp_send_400(req, "Invalid JSON");
//         return ESP_FAIL;
//     }
    
//     daily_schedule_t schedule;
//     strncpy(schedule.date, date, sizeof(schedule.date) - 1);
//     schedule.event_count = 0;
    
//     cJSON* events_array = cJSON_GetObjectItem(root, "events");
//     if (cJSON_IsArray(events_array)) {
//         cJSON* event_item;
//         cJSON_ArrayForEach(event_item, events_array) {
//             if (schedule.event_count >= MAX_SCHEDULES_PER_DAY) break;
            
//             schedule_event_t event = {0};
            
//             cJSON* id = cJSON_GetObjectItem(event_item, "id");
//             cJSON* year = cJSON_GetObjectItem(event_item, "year");
//             cJSON* month = cJSON_GetObjectItem(event_item, "month");
//             cJSON* day = cJSON_GetObjectItem(event_item, "day");
//             cJSON* hour = cJSON_GetObjectItem(event_item, "hour");
//             cJSON* minute = cJSON_GetObjectItem(event_item, "minute");
//             cJSON* duration = cJSON_GetObjectItem(event_item, "duration_seconds");
//             cJSON* enabled = cJSON_GetObjectItem(event_item, "enabled");
            
//             if (cJSON_IsString(id)) {
//                 strncpy(event.id, id->valuestring, sizeof(event.id) - 1);
//             } else {
//                 // Generate unique ID if not provided
//                 snprintf(event.id, sizeof(event.id), "event_%d", schedule.event_count);
//             }
            
//             if (cJSON_IsNumber(year)) event.year = year->valueint;
//             if (cJSON_IsNumber(month)) event.month = month->valueint;
//             if (cJSON_IsNumber(day)) event.day = day->valueint;
//             if (cJSON_IsNumber(hour)) event.hour = hour->valueint;
//             if (cJSON_IsNumber(minute)) event.minute = minute->valueint;
//             if (cJSON_IsNumber(duration)) event.duration_seconds = duration->valueint;
//             if (cJSON_IsBool(enabled)) event.enabled = cJSON_IsTrue(enabled);
//             else event.enabled = true;
            
//             schedule.events[schedule.event_count++] = event;
//         }
//     }
    
//     cJSON_Delete(root);
    
//     if (save_daily_schedule(date, &schedule)) {
//         httpd_resp_send(req, "OK", 2);
//     } else {
//         httpd_resp_send_500(req);
//     }
    
//     return ESP_OK;
// }

// void start_web_server(void)
// {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
//     if (httpd_start(&server, &config) == ESP_OK) {
//         httpd_register_uri_handler(server, &status_get);
//         httpd_register_uri_handler(server, &schedule_post);
//         httpd_register_uri_handler(server, &water_post);
//         ESP_LOGI(TAG, "Web server started");
//     }
// }


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

static const char *TAG = "WEB_SERVER";

static httpd_handle_t server = NULL;

// HTML content for the main page
static const char* HTML_MAIN_PAGE = 
"<!DOCTYPE html>"
"<html lang='en'>"
"<head>"
"    <meta charset='UTF-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"    <title>Plant Watering System</title>"
"    <style>"
"        * { margin: 0; padding: 0; box-sizing: border-box; }"
"        body { font-family: Arial, sans-serif; background: #f0f0f0; padding: 20px; }"
"        .container { max-width: 1200px; margin: 0 auto; }"
"        .header { background: #2c3e50; color: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; }"
"        .manual-section { background: white; padding: 20px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }"
"        .columns { display: flex; gap: 20px; }"
"        .column { flex: 1; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }"
"        .form-group { margin-bottom: 15px; }"
"        label { display: block; margin-bottom: 5px; font-weight: bold; color: #333; }"
"        input, select, button { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; font-size: 14px; }"
"        button { background: #3498db; color: white; border: none; cursor: pointer; margin-top: 10px; }"
"        button:hover { background: #2980b9; }"
"        button.manual { background: #e74c3c; }"
"        button.manual:hover { background: #c0392b; }"
"        .schedule-list { list-style: none; }"
"        .schedule-item { background: #ecf0f1; padding: 10px; margin-bottom: 10px; border-radius: 5px; }"
"        .schedule-time { font-weight: bold; color: #2c3e50; }"
"        .schedule-date { color: #7f8c8d; font-size: 12px; }"
"        .status { padding: 10px; border-radius: 5px; margin-bottom: 10px; text-align: center; }"
"        .status.connected { background: #d4edda; color: #155724; }"
"        .status.error { background: #f8d7da; color: #721c24; }"
"        .quick-buttons { display: flex; gap: 10px; margin-top: 10px; }"
"        .quick-buttons button { flex: 1; }"
"        h2 { color: #2c3e50; margin-bottom: 15px; }"
"        h3 { color: #34495e; margin-bottom: 10px; }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <div class='header'>"
"            <h1>🌱 Plant Watering System</h1>"
"            <div id='status' class='status'></div>"
"        </div>"
"        "
"        <div class='manual-section'>"
"            <h2>Manual Watering Control</h2>"
"            <div class='form-group'>"
"                <label for='duration'>Watering Duration (seconds, max 3600):</label>"
"                <input type='number' id='duration' min='1' max='3600' value='30'>"
"            </div>"
"            <button class='manual' onclick='startManualWatering()'>Start Manual Watering</button>"
"            <div class='quick-buttons'>"
"                <button onclick='setDuration(30)'>30s</button>"
"                <button onclick='setDuration(60)'>1m</button>"
"                <button onclick='setDuration(300)'>5m</button>"
"                <button onclick='setDuration(600)'>10m</button>"
"            </div>"
"        </div>"
"        "
"        <div class='columns'>"
"            <div class='column'>"
"                <h2>Create New Schedule</h2>"
"                <form id='scheduleForm'>"
"                    <div class='form-group'>"
"                        <label for='scheduleDate'>Date:</label>"
"                        <input type='date' id='scheduleDate' required>"
"                    </div>"
"                    <div class='form-group'>"
"                        <label for='scheduleTime'>Time:</label>"
"                        <input type='time' id='scheduleTime' required>"
"                    </div>"
"                    <div class='form-group'>"
"                        <label for='scheduleDuration'>Duration (seconds):</label>"
"                        <input type='number' id='scheduleDuration' min='1' max='3600' value='30' required>"
"                    </div>"
"                    <div class='form-group'>"
"                        <label for='eventId'>Event ID (optional):</label>"
"                        <input type='text' id='eventId' placeholder='morning_watering'>"
"                    </div>"
"                    <button type='button' onclick='createSchedule()'>Create Schedule</button>"
"                </form>"
"            </div>"
"            "
"            <div class='column'>"
"                <h2>Schedules for Next 7 Days</h2>"
"                <div id='schedulesList'></div>"
"            </div>"
"        </div>"
"    </div>"
"    "
"    <script>"
"        function setDuration(seconds) {"
"            document.getElementById('duration').value = seconds;"
"        }"
"        "
"        function startManualWatering() {"
"            const duration = document.getElementById('duration').value;"
"            if (duration < 1 || duration > 3600) {"
"                alert('Duration must be between 1 and 3600 seconds');"
"                return;"
"            }"
"            "
"            fetch('/api/water', {"
"                method: 'POST',"
"                headers: { 'Content-Type': 'application/json' },"
"                body: JSON.stringify({ duration: parseInt(duration) })"
"            })"
"            .then(response => response.text())"
"            .then(data => {"
"                alert('Manual watering started for ' + duration + ' seconds');"
"                updateStatus('Manual watering started', 'connected');"
"            })"
"            .catch(error => {"
"                console.error('Error:', error);"
"                updateStatus('Error starting watering', 'error');"
"            });"
"        }"
"        "
"        function createSchedule() {"
"            const date = document.getElementById('scheduleDate').value;"
"            const time = document.getElementById('scheduleTime').value;"
"            const duration = document.getElementById('scheduleDuration').value;"
"            const eventId = document.getElementById('eventId').value || `event_${Date.now()}`;"
"            "
"            if (!date || !time || !duration) {"
"                alert('Please fill in all required fields');"
"                return;"
"            }"
"            "
"            const [year, month, day] = date.split('-');"
"            const [hour, minute] = time.split(':');"
"            "
"            const scheduleData = {"
"                events: [{"
"                    id: eventId,"
"                    year: parseInt(year),"
"                    month: parseInt(month),"
"                    day: parseInt(day),"
"                    hour: parseInt(hour),"
"                    minute: parseInt(minute),"
"                    duration_seconds: parseInt(duration),"
"                    enabled: true"
"                }]"
"            };"
"            "
"            fetch('/api/schedule?date=' + date, {"
"                method: 'POST',"
"                headers: { 'Content-Type': 'application/json' },"
"                body: JSON.stringify(scheduleData)"
"            })"
"            .then(response => response.text())"
"            .then(data => {"
"                alert('Schedule created successfully!');"
"                document.getElementById('scheduleForm').reset();"
"                loadSchedules();"
"                updateStatus('Schedule created', 'connected');"
"            })"
"            .catch(error => {"
"                console.error('Error:', error);"
"                updateStatus('Error creating schedule', 'error');"
"            });"
"        }"
"        "
"        function loadSchedules() {"
"            const today = new Date();"
"            const schedulesList = document.getElementById('schedulesList');"
"            schedulesList.innerHTML = '<p>Loading schedules...</p>';"
"            "
"            // Get schedules for next 7 days"
"            const promises = [];"
"            for (let i = 0; i < 7; i++) {"
"                const date = new Date(today);"
"                date.setDate(today.getDate() + i);"
"                const dateStr = date.toISOString().split('T')[0];"
"                promises.push(fetch('/api/schedule?date=' + dateStr).then(r => r.json()));"
"            }"
"            "
"            Promise.all(promises)"
"            .then(schedules => {"
"                let html = '<ul class=\"schedule-list\">';"
"                let hasSchedules = false;"
"                "
"                schedules.forEach((schedule, index) => {"
"                    const date = new Date(today);"
"                    date.setDate(today.getDate() + index);"
"                    const dateStr = date.toISOString().split('T')[0];"
"                    "
"                    if (schedule.events && schedule.events.length > 0) {"
"                        hasSchedules = true;"
"                        html += `<li class=\"schedule-item\">"
"                               `<div class=\"schedule-date\">${dateStr}</div>`;"
"                        "
"                        schedule.events.forEach(event => {"
"                            const timeStr = `${event.hour.toString().padStart(2, '0')}:${event.minute.toString().padStart(2, '0')}`;"
"                            html += `<div class=\"schedule-time\">${timeStr} - ${event.duration_seconds}s</div>"
"                                   `<div>ID: ${event.id}</div>`;"
"                        });"
"                        "
"                        html += `</li>`;"
"                    }"
"                });"
"                "
"                if (!hasSchedules) {"
"                    html = '<p>No schedules found for the next 7 days</p>';"
"                } else {"
"                    html += '</ul>';"
"                }"
"                "
"                schedulesList.innerHTML = html;"
"            })"
"            .catch(error => {"
"                console.error('Error loading schedules:', error);"
"                schedulesList.innerHTML = '<p>Error loading schedules</p>';"
"            });"
"        }"
"        "
"        function updateStatus(message, type) {"
"            const statusDiv = document.getElementById('status');"
"            statusDiv.textContent = message;"
"            statusDiv.className = 'status ' + type;"
"            setTimeout(() => {"
"                statusDiv.textContent = '';"
"                statusDiv.className = 'status';"
"            }, 3000);"
"        }"
"        "
"        function checkSystemStatus() {"
"            fetch('/api/status')"
"            .then(response => response.json())"
"            .then(data => {"
"                updateStatus('System connected', 'connected');"
"            })"
"            .catch(error => {"
"                updateStatus('System disconnected', 'error');"
"            });"
"        }"
"        "
"        // Set default date to today and time to next hour"
"        window.onload = function() {"
"            const now = new Date();"
"            const today = now.toISOString().split('T')[0];"
"            const nextHour = (now.getHours() + 1) % 24;"
"            const timeStr = nextHour.toString().padStart(2, '0') + ':00';"
"            "
"            document.getElementById('scheduleDate').value = today;"
"            document.getElementById('scheduleTime').value = timeStr;"
"            "
"            checkSystemStatus();"
"            loadSchedules();"
"            "
"            // Refresh schedules every 30 seconds"
"            setInterval(loadSchedules, 30000);"
"        };"
"    </script>"
"</body>"
"</html>";

// Handler for serving the main page
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_MAIN_PAGE, strlen(HTML_MAIN_PAGE));
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
        snprintf(date, sizeof(date), "%04d-%02d-%02d", 
                 current_time.tm_year + 1900, 
                 current_time.tm_mon + 1, 
                 current_time.tm_mday);
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