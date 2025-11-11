#ifndef __WATER_STATUS_H__
#define __WATER_STATUS_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize UART0, GPIO3 and start periodic update task
void water_status_init(void);

// Read last known water level and empty flag.
// Returns true if values were copied successfully.
bool get_water_level_status(uint16_t *level, uint8_t *empty);

#ifdef __cplusplus
}
#endif

#endif
