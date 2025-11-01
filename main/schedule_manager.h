#ifndef _SCHEDULE_MANAGER_H_
#define _SCHEDULE_MANAGER_H_

#include <stdbool.h>
#include <time.h>

#define MAX_SCHEDULES_PER_DAY 10
#define SCHEDULE_FILE_PATH "/spiffs/schedules"
#define SUMMARY_FILE_PATH "/spiffs/schedules/summary.json"

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int duration_seconds;
    bool enabled;
    char id[32]; // Unique identifier for each schedule
} schedule_event_t;

typedef struct {
    char date[11]; // YYYY-MM-DD format
    int event_count;
    schedule_event_t events[MAX_SCHEDULES_PER_DAY];
} daily_schedule_t;

// Schedule management functions
void schedule_manager_init(void);
bool save_daily_schedule(const char* date, const daily_schedule_t* schedule);
bool load_daily_schedule(const char* date, daily_schedule_t* schedule);
bool delete_daily_schedule(const char* date);
bool add_schedule_event(const char* date, const schedule_event_t* event);
bool remove_schedule_event(const char* date, const char* event_id);
bool update_schedule_event(const char* date, const schedule_event_t* event);

// Summary file functions
bool update_summary_file(void);
bool get_available_dates(char*** dates, int* count);
void free_date_list(char** dates, int count);

// Schedule checking functions
bool check_schedule_event(const schedule_event_t* event, struct tm* current_time);
daily_schedule_t* get_today_schedule(void);
daily_schedule_t* get_tomorrow_schedule(void);

// Utility functions
bool is_valid_date(const char* date);
void date_to_string(struct tm* time, char* buffer, size_t buffer_size);
bool string_to_date(const char* date_str, struct tm* result);

#endif