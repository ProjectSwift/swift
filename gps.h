#ifndef __GPS_H__
#define __GPS_H__

#include <stdbool.h>

void gps_setup(void);
bool gps_get_pos(int32_t* lat, int32_t* lon, int32_t* alt);
bool gps_get_time(uint8_t* hour, uint8_t* minute, uint8_t* second);
bool gps_get_lock(uint8_t* lock, uint8_t* sats);
uint8_t gps_check_nav(void);
bool _gps_verify_checksum(uint8_t* data, uint8_t len);
void gps_ubx_checksum(uint8_t* data, uint8_t len, uint8_t* cka, uint8_t* ckb);
void _gps_send_msg(uint8_t* data, uint8_t len);
uint8_t _gps_get_byte(void);
void _gps_flush_buffer(void);


#endif /*__GPS_H__ */
