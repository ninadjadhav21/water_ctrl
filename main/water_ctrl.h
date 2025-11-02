#ifndef _WATERING_CONTROLLER_H_
#define _WATERING_CONTROLLER_H_

#include <stdbool.h>
#include "schedule_manager.h"

typedef enum {
    MANUAL_WATERING,
    SCHEDULED_WATERING
} watering_type_t;

typedef struct {
    watering_type_t type;
    int duration;
    char event_id[32];
} watering_command_t;

// Basic watering functions
void watering_controller_init(void);
void start_manual_watering(int duration_seconds);
void add_scheduled_watering(const schedule_event_t* event);

// Schedule management functions
bool watering_add_schedule(const char* date, const schedule_event_t* event);
bool watering_remove_schedule(const char* date, const char* event_id);

// Task function
void watering_task(void *pvParameters);

#endif