#ifndef CURRENT_MONITOR_H
#define CURRENT_MONITOR_H

#include <stdint.h>




void current_monitor_init(void);
int current_monitor_read_ma(uint32_t rsense);
#endif