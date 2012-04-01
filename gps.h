#ifndef __GPS_H__
#define __GPS_H__

#include <stdbool.h>

extern void gps_setup(void);
extern bool gps_get_pos(int32_t* lat, int32_t* lon, int32_t* alt);
extern bool gps_get_time(uint8_t* hour, uint8_t* minute, uint8_t* second);
extern bool gps_get_lock(uint8_t* lock, uint8_t* sats);
extern uint8_t gps_check_nav(void);

#endif /*__GPS_H__ */
