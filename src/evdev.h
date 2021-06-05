#pragma once

#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

char* scan_devices(uint16_t, uint16_t);
bool get_events(int fd, uint16_t type, uint16_t* code, uint16_t* value);

#ifdef __cplusplus
};
#endif

