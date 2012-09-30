/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Nigel Smart <nigel@projectswift.co.uk>            */
/* And Philip Heron <phil@sanslogic.co.uk>                               */
/* Additional Help from Jon Sowman & Adam Greig of Cambridge University  */
/* Spaceflight (www.cusf.co.uk)                                          */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program. If not, see <http://www.gnu.org/licenses/>.  */

#ifndef __GPS_H__
#define __GPS_H__

#include <stdint.h>
#include "timeout.h"

#define GPS_OK          (0)
#define GPS_ERROR       (1)
#define GPS_TIMEOUT     (2)
#define GPS_BUFFER_FULL (3)
#define GPS_BAD_CRC     (4)
#define GPS_NAK         (5)
#define GPS_UNEXPECTED  (6)

extern void gps_setup(void);

extern void gps_send_packet(uint8_t class, uint8_t id, uint8_t *payload, uint16_t length);
extern int gps_get_packet(uint8_t *class, uint8_t *id, uint8_t *payload, uint16_t *length, to_int timeout);
extern int gps_get_packet_type(uint8_t class, uint8_t id, uint8_t *payload, uint16_t length, to_int timeout);
extern int gps_get_ack(uint8_t class, uint8_t id, to_int timeout);

extern int gps_get_pos(int32_t *lat, int32_t *lon, int32_t *alt);
extern int gps_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second);
extern int gps_get_lock(uint8_t *lock, uint8_t *sats);

extern int gps_set_nav(uint8_t nav);
extern int gps_get_nav(uint8_t *nav);

#endif /*__GPS_H__ */
