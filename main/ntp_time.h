#ifndef _NTP_TIME_H_
#define _NTP_TIME_H_

#include <time.h>

void ntp_time_init(void);
bool ntp_time_sync(void);
void get_current_time(struct tm* timeinfo);
bool set_time_zone(const char* timezone);
bool is_time_synced(void);
void ntp_time_task(void* pvParameters);

#endif