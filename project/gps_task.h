#ifndef _GPS_
#define _GPS_
#include "FreeRTOS.h"

void init_gps(void);

BaseType_t gps_setTime();

#endif // _GPS_